#ifndef OAI_PROMPT_TEMPLATE_H
#define OAI_PROMPT_TEMPLATE_H

#include "core/string/ustring.h"
#include "core/variant/typed_array.h"

namespace AIPromptTemplate {
    extern const char* SYSTEM_PROMPT;
    String format_prompt(const Dictionary& project_resources, const Dictionary& current_nodes);
};

#endif