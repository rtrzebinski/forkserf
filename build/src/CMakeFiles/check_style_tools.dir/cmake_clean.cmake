FILE(REMOVE_RECURSE
  "CMakeFiles/check_style_tools"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/check_style_tools.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
