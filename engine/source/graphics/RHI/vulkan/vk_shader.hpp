#pragma once

#include "vk_forward.hpp"
#include "../shader.hpp"

#include <core/arena.hpp>

namespace cannele::inline graphics::rhi::vk
{
    struct VulkanShaderObjectLayout;
    struct VulkanEntryPointLayout;
    struct VulkanRootShaderObjectLayout;

    struct VulkanShaderProgram final: RHIShaderProgram
    {
        struct Module final
        {
            std::span<std::byte const> code{};
            std::string entry_point_name{};
            VkShaderModule shader_module{VK_NULL_HANDLE};
        };

        std::unique_ptr<VulkanRootShaderObjectLayout> root_layout{};

        std::vector<Module> modules{};
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};

        VulkanShaderProgram(VulkanDevice* device, ShaderProgramCreateInfo const& info);
        virtual ~VulkanShaderProgram();

        auto create_shader_module(slang::EntryPointReflection* entry_point_info, std::span<std::byte const> code) -> bool override;
        auto root_shader_object_layout() const -> ShaderObjectLayout* override;
    };

    struct BindingOffsetBase
    {
        uint32_t binding{};
        uint32_t set{};
        uint32_t push_constant_range{};

        BindingOffsetBase() = default;
        BindingOffsetBase(slang::VariableLayoutReflection* var_layout)
        {
            if (var_layout) {
                binding = var_layout->getOffset(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT);
                set = var_layout->getBindingSpace(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT);
                push_constant_range = var_layout->getOffset(SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER);
            }
        }

        auto operator += (BindingOffsetBase const& other) -> void
        {
            binding += other.binding;
            set += other.set;
            push_constant_range += other.push_constant_range;
        }
    };

    struct BindingOffset: BindingOffsetBase
    {
        BindingOffsetBase pending{};

        BindingOffset() = default;

        explicit BindingOffset(BindingOffsetBase const& base)
            : BindingOffsetBase(base)
        {}

        BindingOffset(slang::VariableLayoutReflection* var_layout)
            : BindingOffsetBase(var_layout)
            , pending(var_layout->getPendingDataLayout())
        {}

        auto operator += (BindingOffsetBase const& other) -> void
        {
            BindingOffsetBase::operator+=(other);
        }

        auto operator += (BindingOffset const& other) -> void
        {
            BindingOffsetBase::operator+=(other);
            pending += other.pending;
        }
    };

    struct VulkanShaderObjectLayout: ShaderObjectLayout
    {
        using Super = ShaderObjectLayout;

        struct BindingRangeInfo final: Super::BindingRangeInfo
        {
            uint32_t binding_offset{};
            uint32_t set_offset{};
        };

        struct SubObjectRangeOffset final: BindingOffset
        {
            uint32_t pending_ordinary_data{};

            SubObjectRangeOffset() = default;

            SubObjectRangeOffset(slang::VariableLayoutReflection* var_layout)
                : BindingOffset(var_layout)
            {
                if (auto pending_layout = var_layout->getPendingDataLayout()) {
                    pending_ordinary_data = pending_layout->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM);
                }
            }
        };

        struct SubObjectRangeStride final: BindingOffset
        {
            uint32_t pending_ordinary_data{};

            SubObjectRangeStride() = default;

            SubObjectRangeStride(slang::TypeLayoutReflection* type_layout)
            {
                if (auto pending_layout = type_layout->getPendingDataTypeLayout()) {
                    pending_ordinary_data = pending_layout->getStride();
                }
            }
        };

        struct SubObjectRangeInfo final: Super::SubObjectRangeInfo
        {
            std::shared_ptr<VulkanShaderObjectLayout> layout{};

            SubObjectRangeOffset offset{};
            SubObjectRangeStride stride{};
        };

        struct DescriptorSetInfo final
        {
            std::vector<VkDescriptorSetLayoutBinding> bindings{};
            int32_t set_index{-1};
            VkDescriptorSetLayout set_layout{VK_NULL_HANDLE};
        };

        uint32_t slot_count_{};
        uint32_t sub_object_count_{};

        std::vector<DescriptorSetInfo> owned_descriptor_sets{};
        std::vector<BindingRangeInfo> binding_ranges{};
        std::vector<SubObjectRangeInfo> sub_object_ranges{};
        std::vector<VkPushConstantRange> push_constant_ranges{};
        std::unordered_map<uint32_t, uint32_t> set_to_index_map{};

        uint32_t child_push_constant_range_count{};
        uint32_t child_descriptor_set_count{};
        uint32_t total_binding_count{};
        uint32_t total_ordinary_data_size{};

        VulkanShaderObjectLayout(VulkanDevice* device, slang::ISession* session, slang::TypeLayoutReflection* element_type_layout);
        virtual ~VulkanShaderObjectLayout();

        static auto create_element_layout(VulkanDevice* device, slang::ISession* session, slang::TypeLayoutReflection* element_type_layout) -> std::unique_ptr<VulkanShaderObjectLayout>;
        static auto create_entry_point_layout(VulkanDevice* device, slang::ISession* session, slang::EntryPointReflection* entry_point_reflection) -> std::unique_ptr<VulkanEntryPointLayout>;
        static auto create_root_layout(VulkanDevice* device, slang::IComponentType* program, slang::ProgramLayout* program_layout) -> std::unique_ptr<VulkanRootShaderObjectLayout>;

        inline auto slot_count() -> uint32_t override { return slot_count_; }
        inline auto sub_object_count() -> uint32_t override { return sub_object_count_; }
        inline auto binding_range_count() -> uint32_t override { return binding_ranges.size(); }
        inline auto binding_range(uint32_t index) -> BindingRangeInfo const& override { return binding_ranges[index]; }
        inline auto ordinary_data_size() -> uint32_t override { return total_ordinary_data_size; }
        inline auto sub_object_range_count() -> uint32_t override { return sub_object_ranges.size(); }
        inline auto sub_object_range(uint32_t index) -> SubObjectRangeInfo const& override { return sub_object_ranges[index]; }
        inline auto sub_object_range_layout(uint32_t index) -> ShaderObjectLayout* override { return sub_object_ranges[index].layout.get(); }

        inline auto owned_descriptor_set_count() -> uint32_t { return owned_descriptor_sets.size(); }
        inline auto total_descriptor_set_count() -> uint32_t { return child_descriptor_set_count + owned_descriptor_set_count(); }

        inline auto owned_push_constant_range_count() -> uint32_t { return push_constant_ranges.size(); }
        inline auto total_push_constant_range_count() -> uint32_t { return child_push_constant_range_count + owned_push_constant_range_count(); }

        auto find_or_add_descriptor_set(uint32_t set_index) -> uint32_t;

        auto add_descriptor_range_as_value(slang::TypeLayoutReflection* type_layout, BindingOffset const& offset, VkShaderStageFlags stage) -> void;
        auto add_descriptor_range_as_constant_buffer(
            slang::TypeLayoutReflection* type_layout,
            BindingOffset const& container_offset,
            BindingOffset const& element_offset,
            VkShaderStageFlags stage
        ) -> void;
        auto add_descriptor_range_as_push_constant(
            slang::TypeLayoutReflection* type_layout,
            BindingOffset const& container_offset,
            BindingOffset const& element_offset,
            VkShaderStageFlags stage
        ) -> void;

        auto add_binding_ranges(slang::TypeLayoutReflection* type_layout) -> void;

        auto set_element_type_layout(slang::TypeLayoutReflection* type_layout) -> bool;

        auto create_vk_descriptor_set_layout() -> void { } // TODO:
    };

    struct VulkanEntryPointLayout final: VulkanShaderObjectLayout
    {
        using Super = VulkanShaderObjectLayout;

        slang::EntryPointLayout* slang_entry_point_layout{};
        VkShaderStageFlags stage_flag{};

        VulkanEntryPointLayout(VulkanDevice* device, slang::ISession* session)
            : VulkanShaderObjectLayout(device, session, element_type_layout)
        {}
    };

    struct VulkanRootShaderObjectLayout: VulkanShaderObjectLayout
    {
        using Super = VulkanShaderObjectLayout;

        struct EntryPointInfo final
        {
            std::unique_ptr<VulkanEntryPointLayout> layout{};

            BindingOffset offset{};
        };

        slang::IComponentType* program{};
        slang::ProgramLayout* program_layout{};
        std::vector<EntryPointInfo> entry_point_infos{};
        VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
        inplace_vector<VkDescriptorSetLayout> descriptor_set_layouts{};
        std::vector<VkPushConstantRange> all_push_constant_ranges{};
        uint32_t total_push_constant_size{};

        BindingOffsetBase pending_data_offset{};

        VulkanRootShaderObjectLayout(VulkanDevice* device, slang::IComponentType* program, slang::ProgramLayout* program_layout);
        virtual ~VulkanRootShaderObjectLayout();

        auto entry_point_count() -> uint32_t override { return entry_point_infos.size(); }
        auto entry_point_layout(uint32_t index) -> ShaderObjectLayout* override { return entry_point_infos[index].layout.get(); }

        auto add_global_params(slang::VariableLayoutReflection* globals_layout) -> void;

        auto add_entry_point(slang::EntryPointReflection* entry_point_layout) -> void;

        auto add_all_descriptor_sets() -> bool;
        auto add_all_descriptor_sets_recursive(VulkanShaderObjectLayout* layout) -> bool;
        auto add_child_descriptor_set_recursive(VulkanShaderObjectLayout* layout) -> bool;
        auto add_all_push_constant_ranges() -> bool;
        auto add_all_push_constant_ranges_recursive(VulkanShaderObjectLayout* layout) -> bool;
        auto add_child_push_constant_range_recursive(VulkanShaderObjectLayout* layout) -> bool;
    };

    struct VulkanBindingData final: BindingData
    {
        struct BufferState final
        {
            VulkanBuffer* buffer{};
            EResourceStates state{};
        };

        struct TextureState final
        {
            VulkanTextureView* texture_view{};
            EResourceStates state{};
        };

        std::span<BufferState> buffer_states{};
        std::span<TextureState> texture_states{};

        VkPipelineLayout pipeline_layout{};

        std::span<VkPushConstantRange> ranges{};
        std::span<VkPushConstantRange> processed_ranges{};
        std::span<std::span<std::byte>> push_constant_datas{};
    };

    struct BindingCache final
    {
        std::vector<VulkanBindingData*> binding_data{};
    };

    struct BindingDataBuilder final
    {
        VulkanDevice* device{};
        Arena* allocator{};
        BindingCache* binding_cache{};
        VulkanBindingData* binding_data{};

        std::vector<VulkanBindingData::BufferState> buffer_states{};
        std::vector<VulkanBindingData::TextureState> texture_states{};

        auto bind_as_root(RootShaderObject* root_object, VulkanRootShaderObjectLayout* specialized_layout) -> VulkanBindingData*;

        auto bind_as_push_constants(ShaderObject* shader_object, BindingOffset const& offset, VulkanShaderObjectLayout* specialized_layout) -> bool;

        auto bind_as_value(ShaderObject* shader_object, BindingOffset const& offset, VulkanShaderObjectLayout* specialized_layout) -> bool;

        auto bind_ordinary_data_buffer_if_needed(ShaderObject* shader_object, BindingOffset& offset, VulkanShaderObjectLayout* specialized_layout) -> bool;
    };
}
