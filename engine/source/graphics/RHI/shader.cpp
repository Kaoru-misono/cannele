#include "shader.hpp"
#include "device.hpp"

namespace cannele::inline graphics::rhi
{
    RHIShaderProgram::RHIShaderProgram(IDevice* device, ShaderProgramCreateInfo const& create_info)
        : IResource(device)
        , info(create_info)
        , id(device->next_shader_program_id.fetch_add(1))
    {
        root_component = info.root_component;
        entry_points = info.entry_points | std::ranges::to<std::vector>();

        auto components = entry_points | std::ranges::to<std::vector<slang::IComponentType*>>();
        if (root_component) {
            components.emplace_back(root_component);
        }

        auto& session = info.session;
        auto result = session->createCompositeComponentType(components.data(), components.size(), linked_program.writeRef());
        CNE_ASSERT_WITH(result == SLANG_OK, "Failed to create composite component type.");

        // TODO: Specialization.
    }

    RHIShaderProgram::~RHIShaderProgram() {}

    auto RHIShaderProgram::compile_shader() -> bool
    {
        if (compiled) return true;

        auto device = get_device();

        auto compile_entry = [&](slang::EntryPointReflection* entry_point_info, slang::IComponentType* component, uint32_t index) {
            auto spirv = device->entry_point_code_from_shader_cache(
                this,
                component,
                entry_point_info->getNameOverride(),
                index,
                0
            );

            create_shader_module(entry_point_info, spirv);

            if (entry_point_info->getStage() == SLANG_STAGE_MESH) {
                has_mesh_shader = true;
            }

            return spirv;
        };

        auto program_reflection = linked_program->getLayout();
        for (auto i = 0u; i < program_reflection->getEntryPointCount(); i++) {
            compile_entry(program_reflection->getEntryPointByIndex(i), linked_program, i);
        }

        compiled = true;

        return true;
    }

    auto RHIShaderProgram::find_type_by_name(std::string_view name) const -> slang::TypeReflection*
    {
        return linked_program->getLayout()->findTypeByName(name.data());
    }

    ShaderObjectLayout::ShaderObjectLayout(IDevice* device, slang::ISession* session, slang::TypeLayoutReflection* element_type_layout)
        : IResource(device)
        , session(std::move(session))
        , element_type_layout(element_type_layout)
    {}

    ShaderObjectLayout::~ShaderObjectLayout()
    {}

    auto ShaderObjectLayout::entry_point_layout(uint32_t index) -> ShaderObjectLayout*
    {
        return nullptr;
    }

    ShaderObject::ShaderObject(ShaderObjectLayout* layout)
        : layout(layout)
    {
        auto uniform_size = 0zu;
        if (layout->container_type == ShaderObjectType::parameter_block) {
            auto parameter_block_type_layout = layout->element_type_layout;
            uniform_size = parameter_block_type_layout->getSize();
        } else {
            uniform_size = layout->element_type_layout->getSize();
        }

        if (uniform_size > 0) {
            data.resize(uniform_size, std::byte{0});
        }

        resource_slots.resize(layout->slot_count());
        objects.resize(layout->sub_object_count());

        for (
            auto sub_object_range_index = 0u;
            sub_object_range_index < layout->sub_object_range_count();
            sub_object_range_index++
        ) {
            auto sub_object_range = &layout->sub_object_range(sub_object_range_index);
            auto sub_object_layout = layout->sub_object_range_layout(sub_object_range_index);

            if (!sub_object_layout) continue;

            auto binding_range = &layout->binding_range(sub_object_range->binding_range_index);
            for (auto i = 0; i < binding_range->count; i++) {
                // auto sub_object = std::make_shared<ShaderObject>(sub_object_layout);
                data.resize(sub_object_layout->element_type_layout->getSize());
                // if (objects.size() <= binding_range->sub_object_index + i) {
                //     objects.resize(binding_range->sub_object_index + i + 1);
                // }
                // objects[binding_range->sub_object_index + i] = std::move(sub_object);
            }
        }
    }

    ShaderObject::~ShaderObject()
    {}

    auto ShaderObject::element_type_layout() -> slang::TypeLayoutReflection*
    {
        return layout->element_type_layout;
    }

    auto ShaderObject::container_type() -> ShaderObjectType
    {
        return layout->container_type;
    }

    auto ShaderObject::entry_point_count() -> uint32_t
    {
        return 0;
    }

    auto ShaderObject::entry_point(uint32_t index) -> ShaderObject*
    {
        return nullptr;
    }

    auto ShaderObject::collect_specialization_args() -> void
    {
        // TODO:
    }

    auto ShaderObject::write_structured_buffer(slang::TypeLayoutReflection* element_layout, ShaderObjectLayout* specialized_layout) -> BufferHandle
    {
        auto buffer_info = BufferCreateInfo{};
        buffer_info.usage       = EBufferUsage::storage | EBufferUsage::uniform;
        buffer_info.size_bytes  = data.size();
        buffer_info.stride      = element_layout->getSize();
        buffer_info.final_state = EResourceStates::uniform_read;

        // return device.get()->create_buffer("", &buffer_info);
        return nullptr; // TODO:
    }

    auto ShaderObject::set_data(ShaderOffset const& offset, void const* in_data, size_t size) -> void
    {
        auto data_offset = offset.uniform_offset;
        auto data_size = size;

        CNE_ASSERT(data.size() >= data_offset + data_size);

        if ((data_offset + data_size) > data.size()) {
            data_size = data.size() - data_offset;
        }

        std::memcpy(data.data() + data_offset, in_data, data_size);

        version++;
    }

    auto ShaderObject::set_descriptor_handle(ShaderOffset const& offset, DescriptorHandle const& handle) -> void
    {
        if (offset.uniform_offset + 8 > data.size()) return;

        std::memcpy(data.data() + offset.uniform_offset, glm::value_ptr(handle), 8);

        version++;
    }

    auto ShaderObject::set_bindless_buffer(ShaderOffset const& offset, RHIBuffer* buffer, EResourceStates state) -> void
    {
        auto handle = buffer->descriptor_handle();
        set_data(offset, &handle, sizeof(handle));

        pending_buffers.emplace_back(buffer, state);
    }

    auto ShaderObject::set_bindless_texture(ShaderOffset const& offset, RHITextureView* texture_view, EResourceStates state) -> void
    {
        auto handle = texture_view->descriptor_handle();
        set_data(offset, &handle, sizeof(handle));

        pending_texture_views.emplace_back(texture_view, state);
    }

    auto ShaderObject::set_bindless_texture(ShaderOffset const& offset, RHITextureView* texture_view, SamplerHandle sampler, EResourceStates state) -> void
    {
        auto handle = math::uint2{texture_view->descriptor_handle().x, sampler->descriptor_handle().x};
        set_data(offset, &handle, sizeof(handle));

        pending_texture_views.emplace_back(texture_view, state);
    }

    auto ShaderObject::set_object(ShaderOffset const& offset, std::shared_ptr<ShaderObject> object) -> void
    {
        if (!allow_modification) return;

        version++;

        if (layout->container_type != ShaderObjectType::none && layout->container_type != ShaderObjectType::parameter_block) {
            // Case 1:
            // We are setting an element into a `StructuredBuffer` object.
            // We need to hold a reference to the element object, as well as
            // writing uniform data to the plain buffer.
            if (offset.binding_array_index >= objects.size()) {
                objects.resize(offset.binding_array_index + 1);
                auto stride = layout->element_type_layout->getStride();
                data.resize(objects.size() * stride);
            }
            objects[offset.binding_array_index] = object;

            auto kind = layout->element_type_layout->getKind();

            auto payload_offset = offset;
            if (layout->element_type_layout->getKind() == slang::TypeReflection::Kind::Interface) {
                auto existential_type = layout->element_type_layout->getType();
                // TODO:
                // auto concrete_type = object->get_specialized_shader_object_type();
                // set_existential_header(existential_type, concrete_type.slang_type, offset);
                payload_offset.uniform_offset += 16;
            }
            set_data(payload_offset, object->data.data(), object->data.size());
        }

        // Case 2 & 3, setting object as an StructuredBuffer, ConstantBuffer, ParameterBlock or
        // existential value.
        if (offset.binding_range_index >= layout->binding_range_count()) return;

        auto binding_range_index = offset.binding_range_index;
        auto binding_range = &layout->binding_range(binding_range_index);

        if ((binding_range->sub_object_index + offset.binding_array_index) >= objects.size()) {
            objects.resize(binding_range->sub_object_index + offset.binding_array_index + 1);
        }
        objects[binding_range->sub_object_index + offset.binding_array_index] = object;

        switch (binding_range->binding_type) {
            case slang::BindingType::ExistentialValue: {
                auto concrete_type_layout = object->element_type_layout();
                auto concrete_type = concrete_type_layout->getType();

                auto existential_type_layout = layout->element_type_layout->getBindingRangeLeafTypeLayout(binding_range_index);
                auto existential_type = existential_type_layout->getType();

                // TODO:
                // set_existential_header(existential_type, concrete_type, offset);
                //
                // auto payload_offset = offset;
                // payload_offset.uniform_offset += 16;
                // if (value_fit_in_existential_payload(concrete_type_layout, existential_type_layout)) {
                //     set_data(payload_offset, object->data.data(), object->data.size());
                // } else {
                // }
                break;
            }
            case slang::BindingType::MutableRawBuffer:
            case slang::BindingType::RawBuffer: {
                // object->write_structured_buffer(object->element_type_layout(), layout);
                // set_binding(offset, buffer);
                break;
            }
            case slang::BindingType::PushConstant: {
                set_data(offset, object->data.data(), object->data.size());
                break;
            }
            default: break;
        }
    }

    auto ShaderObject::sub_object(ShaderOffset const& offset) -> ShaderObject*
    {
        if (offset.binding_range_index >= layout->binding_range_count()) return nullptr;

        auto binding_range = &layout->binding_range(offset.binding_range_index);

        return objects[binding_range->sub_object_index + offset.binding_array_index].get();
    }

    auto ShaderObject::track_resources(std::unordered_set<std::shared_ptr<IResource>>* resources) -> void
    {
        for (auto& slot: resource_slots) {
            if (slot.resource) {
                resources->insert(slot.resource);
            }
        }

        for (auto& object: objects) {
            if (object) {
                object->track_resources(resources);
            }
        }

        for (auto& [buffer, _]: pending_buffers) {
            if (buffer) {
                resources->insert(buffer->shared_from_this());
            }
        }

        for (auto& [texture_view, _]: pending_texture_views) {
            if (texture_view) {
                resources->insert(texture_view->texture()->shared_from_this());
            }
        }
    }

    RootShaderObject::RootShaderObject(RHIShaderProgram const* program)
        : ShaderObject(program->root_shader_object_layout())
        , program(program)
    {
        auto layout = program->root_shader_object_layout();

        for (auto index = 0zu; index < layout->entry_point_count(); index++) {
            auto entry_point_layout = layout->entry_point_layout(index);
            auto entry_point = std::make_shared<ShaderObject>(entry_point_layout);

            entry_points.emplace_back(entry_point);
        }
    }

    RootShaderObject::~RootShaderObject()
    {}

    auto RootShaderObject::entry_point_count() -> uint32_t
    {
        return entry_points.size();
    }

    auto RootShaderObject::entry_point(uint32_t index) -> ShaderObject*
    {
        return entry_points[index].get();
    }

    auto RootShaderObject::track_resources(std::unordered_set<std::shared_ptr<IResource>>* resources) -> void
    {
        ShaderObject::track_resources(resources);
        for (auto& entry_point : entry_points) {
            entry_point->track_resources(resources);
        }
    }

    auto RootShaderObject::specialized_layout() -> ShaderObjectLayout*
    {
        auto result = program->root_shader_object_layout();
        if (program->is_specializable()) {
            // TODO:
        }

        return result;
    }

    auto ShaderObjectWriter::field_writer(std::string_view name) -> ShaderObjectWriter
    {
        CNE_ASSERT(valid());


        switch (type_layout->getKind()) {
            using enum slang::TypeReflection::Kind;
            case Struct: {
                auto writer = ShaderObjectWriter();
                auto field_index = type_layout->findFieldIndexByName(name.data());
                if (field_index == -1) break;

                auto field_layout = type_layout->getFieldByIndex(field_index);

                writer.base_object = base_object;
                writer.type_layout = field_layout->getTypeLayout();
                writer.offset.uniform_offset      = offset.uniform_offset + field_layout->getOffset();
                writer.offset.binding_range_index = (
                    offset.binding_range_index + type_layout->getFieldBindingRangeOffset(field_index)
                );
                writer.offset.binding_array_index = offset.binding_array_index;

                return writer;
            }
            case ConstantBuffer:
            case ParameterBlock: {
                if (auto sub_object = dereference(); sub_object) {
                    return dereference().field_writer(name);
                }

                break;
            }
            default: break;
        }

        auto entry_point_count = base_object->entry_point_count();
        for (auto entry_point_index = 0u; entry_point_index < entry_point_count; entry_point_index++) {
            auto entry_point = base_object->entry_point(entry_point_index);

            auto entry_point_writer = ShaderObjectWriter{entry_point};

            if (auto writer = entry_point_writer.field_writer(name)) {
                return writer;
            }
        }

        return {};
    }

    auto ShaderObjectWriter::element_writer(uint32_t index) -> ShaderObjectWriter
    {
        if (container_type != ShaderObjectType::none) {
            auto element_writer = ShaderObjectWriter{};
            element_writer.base_object    = base_object;
            element_writer.type_layout    = type_layout->getElementTypeLayout();
            element_writer.container_type = container_type;
            element_writer.offset.uniform_offset      = index * type_layout->getStride();
            element_writer.offset.binding_range_index = 0;
            element_writer.offset.binding_array_index = index;

            return element_writer;
        }

        switch (type_layout->getKind()) {
            using enum slang::TypeReflection::Kind;
            case Array: {
                auto element_writer = ShaderObjectWriter{};
                element_writer.base_object    = base_object;
                // element_writer.type_layout    = type_layout->getElementTypeLayout();
                element_writer.offset.uniform_offset      = (
                    offset.uniform_offset + index * type_layout->getElementStride(SLANG_PARAMETER_CATEGORY_UNIFORM)
                );
                element_writer.offset.binding_range_index = offset.binding_range_index;
                // element_writer.offset.binding_array_index = (
                //     offset.binding_array_index * (uint32_t) type_layout->getElementCount() + index
                // );

                return element_writer;
            }
            case Struct: {
                auto field_index = index;
                auto field_layout = type_layout->getFieldByIndex(field_index);
                if (!field_layout) return {};

                auto field_writer = ShaderObjectWriter{};
                field_writer.base_object    = base_object;
                field_writer.type_layout    = field_layout->getTypeLayout();
                field_writer.offset.uniform_offset      = offset.uniform_offset + field_layout->getOffset();
                field_writer.offset.binding_range_index = (
                    offset.binding_range_index + type_layout->getFieldBindingRangeOffset(field_index)
                );
                field_writer.offset.binding_array_index = offset.binding_array_index;

                return field_writer;
            }
            case Vector:
            case Matrix: {
                auto field_writer = ShaderObjectWriter{};
                field_writer.base_object    = base_object;
                field_writer.type_layout    = type_layout->getElementTypeLayout();
                field_writer.offset.uniform_offset      = (
                    offset.uniform_offset + index * type_layout->getElementStride(SLANG_PARAMETER_CATEGORY_UNIFORM)
                );
                field_writer.offset.binding_range_index = offset.binding_range_index;
                field_writer.offset.binding_array_index = offset.binding_array_index;

                return field_writer;
            }
            default: break;
        }

        return {};
    }

    ShaderObjectWriter::ShaderObjectWriter(ShaderObject* object)
        : base_object(object)
        , type_layout(object->element_type_layout())
        , container_type(object->container_type())
    {}

    auto ShaderObjectWriter::dereference() -> ShaderObjectWriter
    {
        switch (type_layout->getKind()) {
            using enum slang::TypeReflection::Kind;
            case ConstantBuffer:
            case ParameterBlock: {
                auto sub_object = base_object->sub_object(offset);

                if (sub_object) {
                    return ShaderObjectWriter{sub_object};
                }
            }
            default: return {};
        }
    }

    auto ShaderObjectWriter::set_data(void const* data, size_t size) -> void
    {
        CNE_ASSERT(base_object);

        base_object->set_data(offset, data, size);
    }

    auto ShaderObjectWriter::set_object(std::shared_ptr<ShaderObject> object) -> void
    {
        CNE_ASSERT(base_object);

        base_object->set_object(offset, object);
    }

    auto ShaderObjectWriter::set_bindless_buffer(BufferHandle buffer, EResourceStates state) -> void
    {
        CNE_ASSERT(base_object);

        base_object->set_bindless_buffer(offset, buffer.get(), state);
    }

    auto ShaderObjectWriter::set_bindless_texture(RHITextureView* texture_view, EResourceStates state) -> void
    {
        CNE_ASSERT(base_object);

        base_object->set_bindless_texture(offset, texture_view, state);
    }

    auto ShaderObjectWriter::set_bindless_texture(RHITextureView* texture_view, SamplerHandle sampler, EResourceStates state) -> void
    {
        CNE_ASSERT(base_object);

        base_object->set_bindless_texture(offset, texture_view, sampler, state);
    }
}
