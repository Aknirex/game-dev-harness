#include "harness/tool_descriptor.h"

namespace harness {

ToolDescriptor ToolDescriptor::Freeze(const ToolSchema& schema) {
  return {
      .name = schema.name,
      .description = schema.description,
      .category = schema.category,
      .danger_level = schema.danger_level,
      .required_permissions = schema.required_permissions,
      .parameters = schema.parameters,
  };
}

}  // namespace harness
