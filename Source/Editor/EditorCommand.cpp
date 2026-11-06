#include "EditorCommand.h"
#include <Runtime/ComponentRegistry.h>
#include <sstream>

namespace GE::Editor
{
    bool SetComponentPayload(EditorCommand& command, const GE::Runtime::Component& component,
                             const GE::Runtime::ComponentRegistry& registry, const GE::Serialization::Context& context,
                             std::string& out_error)
    {
        const GE::Runtime::ComponentDescriptor* descriptor = registry.Find(component.GetTypeId());
        if (descriptor == nullptr)
        {
            out_error = "Component type is not registered";
            return false;
        }
        std::ostringstream output;
        if (!component.Save(output, context, out_error))
            return false;
        command.component_type_id = descriptor->type_id;
        command.component_version = descriptor->version;
        command.component_payload = output.str();
        out_error.clear();
        return true;
    }
} // namespace GE::Editor
