#pragma once

#include "gltf_asset.hpp"

#include <core/assert.hpp>

#include <meshoptimizer.h>
#include <xxhash.h>
#include <metis.h>
#include <ranges>
#include <queue>
#include <stack>

namespace cannele::inline scene::resource
{
    struct Vertex
    {
        math::float3 position{};
        math::float2 uv_0{};
        math::float3 normal{};
        math::float4 tangent{};
        math::float3 smooth_normal{};
        math::float2 uv_1{};
        math::float4 color{};
    };

    struct Meshlet final
    {
        // From meshopt_Meshlet
        // Offsets within meshlet_vertices and meshlet_triangles arrays with meshlet data
        uint32_t vertex_offset;
        uint32_t triangle_offset;

        // Number of vertices and triangles used in the meshlet; data is stored in consecutive range defined by offset and count
        uint32_t vertex_count;
        uint32_t triangle_count;

        float cone_cutoff{};
        math::float3 cone_axis{};
        math::float3 cone_apex{};

        math::float3 pos_min{};
        math::float3 pos_max{};

        uint32_t lod{};

        float error{-1.0f};

        float parent_error{std::numeric_limits<float>::max()};
        math::float3 parent_position_center{};
        math::float3 cluster_position_center{};

        auto parent_valid() const -> bool
        {
            return parent_error != std::numeric_limits<float>::max();
        }

        auto to_gltf_meshlet(uint32_t data_offset) -> GLTFMeshlet
        {
            auto gltf_meshlet = GLTFMeshlet{};
            gltf_meshlet.vertex_triangle_count = (vertex_count & 0xff) | (triangle_count & 0xff) << 8;
            gltf_meshlet.data_offset           = data_offset;
            gltf_meshlet.pos_min               = pos_min;
            gltf_meshlet.pos_max               = pos_max;
            gltf_meshlet.cone_cutoff           = cone_cutoff;
            gltf_meshlet.cone_axis             = cone_axis;
            gltf_meshlet.cone_apex             = cone_apex;
            gltf_meshlet.lod                   = lod;

            return gltf_meshlet;
        }
    };

    struct MeshletContainer final
    {
        std::vector<uint8_t> triangles{};
        std::vector<uint32_t> vertices{};

        std::vector<Meshlet> meshlets{};
        std::vector<GLTFMeshletGroup> meshlet_groups{};
        std::vector<uint32_t> meshlet_group_indices{};
        std::vector<GLTFBVHNode> bvh_nodes{};

        auto append(MeshletContainer&& other) -> void
        {
            auto base_triangle_offset = (uint32_t) triangles.size();
            auto base_vertex_offset   = (uint32_t) vertices.size();

            for (auto& meshlet : other.meshlets)
            {
                meshlet.vertex_offset += base_vertex_offset;
                meshlet.triangle_offset += base_triangle_offset;
            }

            triangles.append_range(std::move(other.triangles));
            vertices.append_range(std::move(other.vertices));
            meshlets.append_range(std::move(other.meshlets));
        }
    };

    // Group-merge-simplify-split parameters.
    static constexpr auto k_min_num_meshlets_per_group = 2u;
    static constexpr auto k_max_num_meshlets_per_group = 4u;
    static constexpr auto k_num_group_splits_after_simplify = 2u;
    static constexpr auto k_group_simplify_threshold = 1.0f / k_num_group_splits_after_simplify;

    // Simplify error relative to extent.
    static constexpr auto k_simplify_error_min = 0.01f;
    static constexpr auto k_simplify_error_max = 0.10f;

    // At least next level lod need to reduce 20% triangles, otherwise it is no meaning to store it.
    static constexpr auto k_group_simplify_min_reduce = 0.8f;

    // Group merge position error, relative to simplify error.
    static constexpr auto k_group_merge_pos_error = 0.1f;

    struct MeshLetEdge final
    {
        uint32_t a{};
        uint32_t b{};

        explicit MeshLetEdge(uint32_t in_a, uint32_t in_b)
            : a(std::min(in_a, in_b))
            , b(std::max(in_a, in_b))
        {}

        auto operator <=> (MeshLetEdge const& other) const = default;

        struct Hash final
        {
            auto operator() (MeshLetEdge const& edge) const -> size_t
            {
                return (((size_t) edge.b << 32) | (size_t) edge.a);
            }
        };
    };

    using MeshletIndex = uint32_t;

    struct NaniteBuilder final
    {
        float cone_weight{};

        std::vector<uint32_t> indices{};
        std::vector<Vertex> vertices{};

        auto remap(bool fuse, bool ignore_normals) -> void
        {
            if (fuse) {
                auto remapped_vertices = std::vector<Vertex>{};
                auto remapped_indices  = std::vector<uint32_t>{};
                remapped_vertices.reserve(vertices.size());
                remapped_indices.reserve(indices.size());

                auto vertices_map = std::map<uint64_t, size_t>{};

                //TODO:
            }

            auto index_count = indices.size();

            auto vertex_remap_table = std::vector<uint32_t>(index_count);
            auto vertex_count = meshopt_generateVertexRemap(
                vertex_remap_table.data(),
                indices.data(),
                index_count,
                vertices.data(),
                index_count,
                sizeof(Vertex)
            );

            auto remapped_vertices = std::vector<Vertex>(vertex_count);
            auto remapped_indices  = std::vector<uint32_t>(index_count);

            meshopt_remapVertexBuffer(remapped_vertices.data(), vertices.data(), index_count, sizeof(Vertex), vertex_remap_table.data());
            meshopt_remapIndexBuffer(remapped_indices.data(), indices.data(), index_count, vertex_remap_table.data());

            meshopt_optimizeVertexCache(remapped_indices.data(), remapped_indices.data(), index_count, vertex_count);
            meshopt_optimizeVertexFetch(remapped_vertices.data(), remapped_indices.data(), index_count, remapped_vertices.data(), vertex_count, sizeof(Vertex));

            indices = std::move(remapped_indices);
            vertices = std::move(remapped_vertices);
        }

        auto build() -> MeshletContainer
        {
            auto result = MeshletContainer{};

            auto simplify_scale = meshopt_simplifyScale(glm::value_ptr(vertices[0].position), vertices.size(), sizeof(Vertex));

            auto current_lod_container = build_meshlets(vertices, indices, cone_weight, 0, -1.0f, math::float3{0.0f});

            for (auto lod: std::views::iota(0u, (uint32_t) (NANITE_MAX_LOD_COUNT - 1))) {
                auto const target_lod = lod + 1;
                auto const target_lod_error = (float) lod / (float) NANITE_MAX_LOD_COUNT;

                auto absolute_lod_error = glm::lerp(k_simplify_error_min, k_simplify_error_max, target_lod_error) * simplify_scale;
                auto next_lod_container = meshlets_GMSS(&current_lod_container, absolute_lod_error, target_lod);

                result.append(std::move(current_lod_container));

                if (next_lod_container.meshlets.empty()) break;

                current_lod_container = std::move(next_lod_container);
            }

            // build bvh
            build_bvh_tree(&result);

            return result;
        }

        static auto build_meshlets(std::span<Vertex> vertices, std::span<uint32_t> indices, float cone_weight, uint32_t lod, float error, math::float3 cluster_position_center) -> MeshletContainer
        {
            auto result = MeshletContainer{};

            auto const num_vertices = (uint32_t) vertices.size();
            auto meshlets = std::vector<meshopt_Meshlet>{meshopt_buildMeshletsBound(
                indices.size(),
                NANITE_MESHLET_MAX_VERTEX_COUNT,
                NANITE_MESHLET_MAX_TRIANGLE_COUNT
            )};
            {
                result.vertices.resize(meshlets.size() * NANITE_MESHLET_MAX_VERTEX_COUNT);
                result.triangles.resize(meshlets.size() * NANITE_MESHLET_MAX_TRIANGLE_COUNT * 3);

                auto meshlet_size = meshopt_buildMeshlets(
                    meshlets.data(),
                    result.vertices.data(),
                    result.triangles.data(),
                    indices.data(),
                    indices.size(),
                    glm::value_ptr(vertices[0].position),
                    num_vertices,
                    sizeof(Vertex),
                    NANITE_MESHLET_MAX_VERTEX_COUNT,
                    NANITE_MESHLET_MAX_TRIANGLE_COUNT,
                    cone_weight
                );
                meshlets.resize(meshlet_size);
            }

            result.meshlets = {};
            result.meshlets.reserve(meshlets.size());

            std::ranges::transform(
                meshlets,
                std::back_inserter(result.meshlets),
                [&](meshopt_Meshlet const& meshlet) -> Meshlet {
                    meshopt_optimizeMeshlet(
                        &result.vertices[meshlet.vertex_offset],
                        &result.triangles[meshlet.triangle_offset],
                        meshlet.triangle_count,
                        meshlet.vertex_count
                    );

                    auto bounds = meshopt_computeMeshletBounds(
                        &result.vertices[meshlet.vertex_offset],
                        &result.triangles[meshlet.triangle_offset],
                        meshlet.triangle_count,
                        glm::value_ptr(vertices[0].position),
                        num_vertices,
                        sizeof(Vertex)
                    );

                    auto pos_min = math::float3{std::numeric_limits<float>::max()};
                    auto pos_max = math::float3{std::numeric_limits<float>::min()};

                    for (auto triangle_id: std::views::iota(0zu, meshlet.triangle_count)) {
                        for (auto vertex_id: std::views::iota(0zu, 3zu)) {
                            auto id = result.triangles[meshlet.triangle_offset + triangle_id * 3 + vertex_id];
                            auto vid = result.vertices[meshlet.vertex_offset + id];
                            pos_min = glm::min(pos_min, vertices[vid].position);
                            pos_max = glm::max(pos_max, vertices[vid].position);
                        }
                    }

                    auto const pos_center = (pos_min + pos_max) * 0.5f;
                    auto const radius = glm::length(pos_max - pos_center);

                    return Meshlet{
                        .vertex_offset           = meshlet.vertex_offset,
                        .triangle_offset         = meshlet.triangle_offset,
                        .vertex_count            = meshlet.vertex_count,
                        .triangle_count          = meshlet.triangle_count,
                        .cone_cutoff             = bounds.cone_cutoff,
                        .cone_axis               = glm::make_vec3(bounds.cone_axis),
                        .cone_apex               = glm::make_vec3(bounds.cone_apex),
                        .pos_min                 = pos_min,
                        .pos_max                 = pos_max,
                        .lod                     = lod,
                        .error                   = error,
                        .parent_error            = std::numeric_limits<float>::max(),
                        .parent_position_center  = cluster_position_center,
                        .cluster_position_center = cluster_position_center,
                    };
                }
            );

            return result;
        }

        struct ClusterGroup final
        {
            std::vector<MeshletIndex> meshlet_indices{};

            math::float3 cluster_position_center{};
            float parent_error{};

            math::float3 parent_position_center{};
            float error{};
        };

        struct ClusterParentErrorBVHTree final
        {
            struct Node final
            {
                math::float3 min_pos{};
                math::float3 max_pos{};

                auto sphere_build() -> math::float4
                {
                    return math::float4{(min_pos + max_pos) * 0.5f, glm::length(max_pos - min_pos) * 0.5f};
                }

                std::array<std::unique_ptr<Node>, NANITE_BVH_LEVEL_NODE_COUNT> children{};
                std::vector<ClusterGroup> leaves{};

                uint32_t flatten_index = std::numeric_limits<uint32_t>::max();
                uint32_t depth = 0;
            };

            std::unique_ptr<Node> root{};
        };

        static auto build_bvh(MeshletContainer const* container, ClusterParentErrorBVHTree::Node* root, std::vector<ClusterGroup const*>&& root_node_cluster_group_bounds) -> void
        {
            struct NodeAndClusterGroups final
            {
                std::vector<ClusterGroup const*> cluster_group_bounds{};
                ClusterParentErrorBVHTree::Node* node{};
            };

            auto node_queue = std::queue<NodeAndClusterGroups>{};
            node_queue.push({.cluster_group_bounds = std::move(root_node_cluster_group_bounds), .node = root});

            while (!node_queue.empty()) {
                auto current_node_ptr = node_queue.front().node;
                auto cluster_group_in_bounds = std::move(node_queue.front().cluster_group_bounds);

                node_queue.pop();

                if (cluster_group_in_bounds.empty()) continue;

                // Not enough node to split, build leaves.
                if (cluster_group_in_bounds.size() < NANITE_BVH_LEVEL_NODE_COUNT || (current_node_ptr->depth == NANITE_BVH_LEVEL_NODE_COUNT - 1)) {
                    for (auto const& cluster_ptr: cluster_group_in_bounds) {
                        current_node_ptr->leaves.emplace_back(*cluster_ptr);
                    }
                    continue;
                }

                auto get_longest_axis_index = [](math::float3 const& min, math::float3 const& max) -> uint32_t {
                    auto longest_axis = 0u;
                    auto extent = max - min;
                    if (extent.y >= extent.x && extent.y >= extent.z) {
                        longest_axis = 1u;
                    }
                    if (extent.z >= extent.x && extent.z >= extent.y) {
                        longest_axis = 2u;
                    }
                    return longest_axis;
                };

                auto get_longest_axis_sort_groups = [](uint32_t longest_axis, std::vector<ClusterGroup const*>&& in_groups_in_bound) {
                    auto groups_in_bound = in_groups_in_bound;

                    // Sort group on longest axis.
                    auto meshlet_center_in_longest_axis_sort = std::vector<std::pair<ClusterGroup const*, float>>{};
                    for (auto group_ptr: groups_in_bound) {
                        meshlet_center_in_longest_axis_sort.emplace_back(std::make_pair(group_ptr, group_ptr->parent_position_center[longest_axis]));
                    }
                    std::ranges::sort(meshlet_center_in_longest_axis_sort, [](auto const& a, auto const& b) {
                        return a.second < b.second;
                    });

                    return std::move(meshlet_center_in_longest_axis_sort);
                };

                // Split 2x2x2.
                {
                    auto longest_0   = get_longest_axis_index(current_node_ptr->min_pos, current_node_ptr->max_pos);
                    auto sort_0      = get_longest_axis_sort_groups(longest_0, std::move(cluster_group_in_bounds));

                    auto process_group = [&](auto& sort_groups, auto start_gid, auto end_gid) {
                        auto iter_groups = sort_groups
                            | std::views::drop(start_gid)
                            | std::views::take(end_gid - start_gid)
                            | std::views::transform([&](auto&& group) {
                                return group.first;
                            })
                            | std::ranges::to<std::vector>();

                        auto [pos_min, pos_max] = std::ranges::fold_left(
                            iter_groups,
                            std::make_pair(
                                math::float3{std::numeric_limits<float>::max()},
                                math::float3{std::numeric_limits<float>::min()}
                            ),
                            [](auto&& acc, auto&& group) {
                                auto&& [min_pos, max_pos] = acc;
                                return std::make_pair(
                                    glm::min(min_pos, group->parent_position_center - group->parent_error),
                                    glm::max(max_pos, group->parent_position_center + group->parent_error)
                                );
                            }
                        );

                        return std::make_tuple(
                            pos_min,
                            pos_max,
                            std::move(iter_groups)
                        );
                    };

                    // Split half.
                    for (auto i: std::views::iota(0u, 2u)) {

                        auto [pos_min_0, pos_max_0, iter_groups_0] = process_group(sort_0, i * sort_0.size() / 2, (i + 1) * sort_0.size() / 2);

                        auto longest_1   = get_longest_axis_index(pos_min_0, pos_max_0);
                        auto sort_1      = get_longest_axis_sort_groups(longest_1, std::move(iter_groups_0));

                        for (auto j: std::views::iota(0u, 2u)) {
                            auto [pos_min_1, pos_max_1, iter_groups_1] = process_group(sort_1, j * sort_1.size() / 2, (j + 1) * sort_1.size() / 2);

                            auto longest_2   = get_longest_axis_index(pos_min_1, pos_max_1);
                            auto sort_2      = get_longest_axis_sort_groups(longest_2, std::move(iter_groups_1));
                            auto sort_2_size = sort_2.size();

                            for (auto k: std::views::iota(0u, 2u)) {
                                auto [pos_min_2, pos_max_2, iter_groups_2] = process_group(sort_2, k * sort_2_size / 2, (k + 1) * sort_2_size / 2);

                                auto node_id = (i * 2 + j) * 2 + k;

                                auto& child = current_node_ptr->children[node_id];
                                child = std::make_unique<ClusterParentErrorBVHTree::Node>();
                                child->depth = current_node_ptr->depth + 1;

                                child->min_pos = pos_min_2;
                                child->max_pos = pos_max_2;

                                node_queue.push({.cluster_group_bounds = std::move(iter_groups_2), .node = child.get()});
                            }
                        }
                    }
                }
            }
        }

        static auto flatten_bvh(MeshletContainer* container, ClusterParentErrorBVHTree::Node* root) -> void
        {
            auto node_queue = std::queue<ClusterParentErrorBVHTree::Node*>{};
            node_queue.push(root);

            auto meshlet_groups        = &container->meshlet_groups;
            auto meshlet_group_indices = &container->meshlet_group_indices;
            auto flatten_node          = &container->bvh_nodes;

            while (!node_queue.empty()) {
                auto node = node_queue.front();
                {
                    auto current_index = flatten_node->size();
                    auto current_node = &flatten_node->emplace_back();

                    current_node->sphere = node->sphere_build();

                    // Prepare leaves meshlet.
                    current_node->leaf_meshlet_group_offset = meshlet_groups->size();
                    current_node->leaf_meshlet_group_count  = node->leaves.size();

                    for (auto&& group: node->leaves) {
                        auto new_group = GLTFMeshletGroup{};
                        new_group.cluster_position_center = group.cluster_position_center;
                        new_group.error                   = group.error;
                        new_group.parent_error            = group.parent_error;
                        new_group.parent_position_center  = group.parent_position_center;
                        new_group.meshlet_count           = group.meshlet_indices.size();
                        new_group.meshlet_offset          = meshlet_group_indices->size();

                        meshlet_group_indices->append_range(group.meshlet_indices);
                        meshlet_groups->emplace_back(std::move(new_group));
                    }

                    node->flatten_index = current_index;
                }
                node_queue.pop();

                for (auto& child: node->children) {
                    if (child) {
                        node_queue.push(child.get());
                    }
                }
            }

            auto count_node_stack = std::stack<ClusterParentErrorBVHTree::Node*>{};
            count_node_stack.push(root);

            node_queue.push(root);
            while (!node_queue.empty()) {
                auto node = node_queue.front();
                node_queue.pop();

                auto node_flatten = &flatten_node->at(node->flatten_index);
                for (auto i: std::views::iota(0u, (uint32_t) NANITE_BVH_LEVEL_NODE_COUNT)) {
                    if (auto& child = node->children[i]) {
                        node_flatten->children[i] = child->flatten_index;
                        node_queue.push(child.get());
                        count_node_stack.push(child.get());
                    } else {
                        node_flatten->children[i] = std::numeric_limits<uint32_t>::max();
                    }
                }
            }

            while (!count_node_stack.empty()) {
                auto node = count_node_stack.top();
                count_node_stack.pop();

                auto node_flatten = &flatten_node->at(node->flatten_index);
                node_flatten->bvh_node_count = 1;

                for (auto& child: node->children | std::views::take(NANITE_BVH_LEVEL_NODE_COUNT)) {
                    auto const child_0_valid_state = node->children[0] != nullptr;
                    auto const current_state_valid = child != nullptr;
                    CNE_ASSERT(child_0_valid_state == current_state_valid);

                    if (child) {
                        auto node_flatten_left = &flatten_node->at(child->flatten_index);
                        node_flatten->bvh_node_count += node_flatten_left->bvh_node_count;
                    }
                }
            }

        }

        static auto build_bvh_tree(MeshletContainer* container) -> void
        {
            auto error_sphere_group_map = std::unordered_map<uint64_t, uint32_t>{};
            auto parent_valid_meshlets = std::vector<ClusterGroup>{};

            auto bvh = ClusterParentErrorBVHTree{};

            //0. Combine all meshlet find max bounds.
            {
                bvh.root = std::make_unique<ClusterParentErrorBVHTree::Node>();
                bvh.root->depth = 0;

                auto root_sphere_group_map = std::unordered_map<uint64_t, uint32_t>{};

                auto pos_min = math::float3{std::numeric_limits<float>::max()};
                auto pos_max = math::float3{std::numeric_limits<float>::min()};

                for (auto meshlet_index: std::views::iota(0u, container->meshlets.size())) {
                    auto meshlet = &container->meshlets[meshlet_index];
                    auto meshlet_hash = hash_meshlet_group(meshlet);

                    auto new_group = ClusterGroup{};
                    new_group.cluster_position_center = meshlet->cluster_position_center;
                    new_group.parent_error            = meshlet->parent_error;
                    new_group.parent_position_center  = meshlet->parent_position_center;
                    new_group.error                   = meshlet->error;
                    new_group.meshlet_indices.emplace_back(meshlet_index);

                    if (meshlet->parent_valid()) {
                        if (error_sphere_group_map.contains(meshlet_hash)) {
                            if (parent_valid_meshlets[error_sphere_group_map[meshlet_hash]].meshlet_indices.size() >= CLUSTER_GROUP_MERGE_MAX_COUNT) {
                                error_sphere_group_map[meshlet_hash] = parent_valid_meshlets.size();
                                parent_valid_meshlets.emplace_back(std::move(new_group));
                            } else {
                                parent_valid_meshlets[error_sphere_group_map[meshlet_hash]].meshlet_indices.emplace_back(meshlet_index);
                            }
                        } else {
                            pos_min = glm::min(pos_min, meshlet->parent_position_center - meshlet->parent_error);
                            pos_max = glm::max(pos_max, meshlet->parent_position_center + meshlet->parent_error);

                            error_sphere_group_map[meshlet_hash] = parent_valid_meshlets.size();
                            parent_valid_meshlets.emplace_back(std::move(new_group));
                        }
                    } else {
                        if (root_sphere_group_map.contains(meshlet_hash)) {
                            if (bvh.root->leaves[root_sphere_group_map[meshlet_hash]].meshlet_indices.size() >= CLUSTER_GROUP_MERGE_MAX_COUNT) {
                                root_sphere_group_map[meshlet_hash] = bvh.root->leaves.size();
                                bvh.root->leaves.emplace_back(std::move(new_group));
                            } else {
                                bvh.root->leaves[root_sphere_group_map[meshlet_hash]].meshlet_indices.emplace_back(meshlet_index);
                            }
                        } else {
                            root_sphere_group_map[meshlet_hash] = bvh.root->leaves.size();
                            bvh.root->leaves.emplace_back(std::move(new_group));
                        }
                    }
                }

                // Root fill.
                bvh.root->min_pos = pos_min;
                bvh.root->max_pos = pos_max;
            }

            //1. Recursive build bvh.
            {
                auto cluter_group_in_bounds = parent_valid_meshlets
                    | std::views::transform([](ClusterGroup const& group) { return &group; })
                    | std::ranges::to<std::vector<ClusterGroup const*>>();

                build_bvh(container, bvh.root.get(), std::move(cluter_group_in_bounds));
            }

            //2. Flatten bvh.
            flatten_bvh(container, bvh.root.get());

            for (auto const& group: container->meshlet_groups) {
                CNE_ASSERT(group.meshlet_count <= CLUSTER_GROUP_MERGE_MAX_COUNT);
            }
            CNE_ASSERT(container->bvh_nodes[0].bvh_node_count == container->bvh_nodes.size());
        }

        static auto hash_meshlet_group(Meshlet const* meshlet) -> uint64_t
        {
            auto errors = std::array<math::float4, 2>{};
            errors[0] = math::float4{meshlet->cluster_position_center, meshlet->error};
            errors[1] = math::float4{meshlet->parent_position_center, meshlet->parent_error};

            return XXH64(errors.data(), errors.size() * sizeof(math::float4), 0);
        }

        using CombineMeshlets = std::vector<uint32_t>;

        auto meshlets_GMSS(MeshletContainer* src, float target_error, uint32_t lod) -> MeshletContainer
        {
            auto result = MeshletContainer{};

            auto const pos_fuse_threshold = k_group_merge_pos_error * 2.0f;
            auto cluster_groups = build_cluster_group(src, vertices, pos_fuse_threshold);

            if (cluster_groups.empty()) {
                return result;
            }

            // Merge simplify split.
            for (auto const& cluster_group: cluster_groups) {
                auto group_merge_vertces = std::vector<uint32_t>{};
                for (auto meshlet_index: cluster_group) {
                    auto meshlet = &src->meshlets[meshlet_index];
                    for (auto triangle_index: std::views::iota(0u, meshlet->triangle_count)) {
                        for (auto i: std::views::iota(0u, 3u)) {
                            group_merge_vertces.emplace_back(get_vertex_index(src, meshlet, triangle_index, i));
                        }
                    }
                }

                // Simplify group.
                auto simplified_vertices = std::vector<uint32_t>(group_merge_vertces.size());
                auto simplification_error = 0.0f;
                {
                    auto const target_indices_count = group_merge_vertces.size() * k_group_simplify_threshold;
                    auto options = meshopt_SimplifyLockBorder | meshopt_SimplifySparse | meshopt_SimplifyErrorAbsolute;

                    constexpr auto attribute_count = 9u;
                    static auto const attribute_weights = std::array<float, attribute_count>{
                        0.05f, 0.05f, // uv.
                        0.5f, 0.5f, 0.5f, // normal.
                        0.001f, 0.001f, 0.001f, 0.05f, // tangent, .w is sign, weight bigger.
                    };

                    auto vertices_count = meshopt_simplifyWithAttributes(
                        simplified_vertices.data(),
                        group_merge_vertces.data(),
                        group_merge_vertces.size(),
                        glm::value_ptr(vertices[0].position),
                        vertices.size(),
                        sizeof(Vertex),
                        glm::value_ptr(vertices[0].uv_0),
                        sizeof(Vertex),
                        attribute_weights.data(),
                        attribute_count,
                        nullptr,
                        target_indices_count,
                        target_error,
                        options,
                        &simplification_error
                    );
                    simplified_vertices.resize(vertices_count);
                }

                // Split - half meshlet.
                auto const traget_indices_min = group_merge_vertces.size() * k_group_simplify_min_reduce;
                if (simplified_vertices.size() > 0 && simplified_vertices.size() < traget_indices_min) {
                    auto passed_error = 0.0f;
                    for (auto smid: cluster_group) {
                        passed_error = std::max(passed_error, src->meshlets[smid].error);
                    }

                    auto const cluster_error = passed_error + simplification_error;
                    {
                        auto pos_min = math::float3{std::numeric_limits<float>::max()};
                        auto pos_max = math::float3{std::numeric_limits<float>::min()};

                        for (auto vid: simplified_vertices) {
                            pos_min = glm::min(pos_min, vertices[vid].position);
                            pos_max = glm::max(pos_max, vertices[vid].position);
                        }

                        auto const cluster_pos_center = (pos_min + pos_max) * 0.5f;

                        for (auto smid: cluster_group) {
                            src->meshlets[smid].parent_error = cluster_error;
                            src->meshlets[smid].parent_position_center = cluster_pos_center;
                        }

                        result.append(build_meshlets(vertices, simplified_vertices, cone_weight, lod, cluster_error, cluster_pos_center));
                    }
                }
            }

            return result;
        }

        static auto get_vertex_index(MeshletContainer const* container, Meshlet const* meshlet, uint32_t triangle_id, uint32_t vertex_id) -> uint32_t
        {
            auto id = container->triangles[meshlet->triangle_offset + triangle_id * 3 + vertex_id];
            return container->vertices[meshlet->vertex_offset + id];
        }

        static auto hash_pos(math::float3 position, float pos_fuse_threshold) -> uint32_t
        {
            auto pos_int3 = glm::ceil(position / pos_fuse_threshold);
            return XXH32(glm::value_ptr(position), sizeof(pos_int3), 0);
        }

        static auto build_cluster_group(MeshletContainer const* container, std::span<Vertex> const vertices, float pos_fuse_threshold) -> std::vector<CombineMeshlets>
        {
            auto const meshlets = &container->meshlets;

            auto build_one_group = [](MeshletContainer const* container) -> CombineMeshlets {
                return std::views::iota(0u, container->meshlets.size()) | std::ranges::to<CombineMeshlets>();
            };

            if (meshlets->size() < k_min_num_meshlets_per_group) {
                return {build_one_group(container)};
            }

            auto const group_meshlet_count = glm::min((uint32_t) meshlets->size() / k_min_num_meshlets_per_group, k_max_num_meshlets_per_group);

            auto edges_to_meshlets = std::unordered_map<MeshLetEdge, std::unordered_set<MeshletIndex>, MeshLetEdge::Hash>{};
            auto meshlets_to_edges = std::unordered_map<MeshletIndex, std::unordered_set<MeshLetEdge, MeshLetEdge::Hash>>{};

            for (auto meshlet_index: std::views::iota(0u, meshlets->size())) {
                auto meshlet = &meshlets->at(meshlet_index);

                for (auto triangle_index: std::views::iota(0u, meshlet->triangle_count)) {
                    for (auto i: std::views::iota(0u, 3u)) {
                        auto v0 = get_vertex_index(container, meshlet, triangle_index, i);
                        auto v1 = get_vertex_index(container, meshlet, triangle_index, (i + 1) % 3);

                        auto edge = MeshLetEdge{hash_pos(vertices[v0].position, pos_fuse_threshold), hash_pos(vertices[v1].position, pos_fuse_threshold)};

                        edges_to_meshlets[edge].insert(meshlet_index);
                        meshlets_to_edges[meshlet_index].insert(edge);
                    }
                }
            }

            // Remove edges which are not connected to 2 different meshlets.
            std::erase_if(edges_to_meshlets, [&](auto const& pair) { return pair.second.size() <= 1; });

            if (edges_to_meshlets.empty()) {
                return {build_one_group(container)};
            }

            // Metis group partitioning.
            auto groups = std::vector<CombineMeshlets>{};
            {
                idx_t vertex_count = meshlets->size();

                auto xadjacency = std::vector<idx_t>{};
                xadjacency.reserve(vertex_count + 1);

                auto edge_adjacency = std::vector<idx_t>{};
                auto edge_weights = std::vector<idx_t>{};

                for (auto meshlet_index: std::views::iota(0u, meshlets->size())) {
                    auto edge_adjacency_offset = edge_adjacency.size();
                    xadjacency.emplace_back(edge_adjacency_offset);

                    std::ranges::for_each(
                        meshlets_to_edges[meshlet_index] | std::views::filter([&](auto const& edge) { return edges_to_meshlets.contains(edge); }),
                        [&](MeshLetEdge const& edge) {
                            auto const& connection = edges_to_meshlets[edge];

                            for (auto const& connected_meshlet: connection) {
                                // Only use other meshlets.
                                if (connected_meshlet != meshlet_index) {
                                    auto existing_edge_it = std::find(edge_adjacency.begin() + edge_adjacency_offset, edge_adjacency.end(), connected_meshlet);
                                    if (existing_edge_it == edge_adjacency.end()) {
                                        CNE_ASSERT_WITH(edge_adjacency.size() == edge_weights.size(), "Edge weights and edge adjacency should have the same length.");
                                        edge_adjacency.emplace_back(connected_meshlet);
                                        edge_weights.emplace_back(1);
                                    } else {
                                        auto diff = existing_edge_it - edge_adjacency.begin();
                                        CNE_ASSERT_WITH(diff >= 0 && diff < edge_weights.size(), "Edge weights and edge adjacency should have the same length.");
                                        edge_weights[diff]++;
                                    }
                                }
                            }
                        }
                    );
                }

                xadjacency.emplace_back(edge_adjacency.size());
                CNE_ASSERT_WITH(xadjacency.size() == vertex_count + 1, "Unexpected count of vertices for METIS graph.");

                auto options = std::array<idx_t, METIS_NOPTIONS>{};
		        METIS_SetDefaultOptions(options.data());
                options[METIS_OPTION_OBJTYPE]   = METIS_OBJTYPE_CUT;
		        options[METIS_OPTION_CCORDER]   = 1; // identify connected components first
		        options[METIS_OPTION_NUMBERING] = 0;

                auto edge_cut = idx_t{};
                auto ncon = idx_t{1};
                auto nparts = (idx_t) (meshlets->size() / group_meshlet_count);
                auto partition = std::vector<idx_t>(vertex_count);

                auto metis_part_result = METIS_PartGraphKway(
                    &vertex_count,
                    &ncon,
                    xadjacency.data(),
                    edge_adjacency.data(),
                    nullptr,
                    nullptr,
                    edge_weights.data(),
                    &nparts,
                    nullptr,
                    nullptr,
                    options.data(),
                    &edge_cut,
                    partition.data()
                );
                CNE_ASSERT_WITH(metis_part_result == METIS_OK, "METIS partitioning failed.");

                groups.resize(nparts);
                for (auto i: std::views::iota(0zu, meshlets->size())) {
                    groups[partition[i]].emplace_back(i);
                }

            }

            return groups;
        }
    };
}
