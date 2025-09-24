# Contributing

## Coding Style

The project uses `.clang-format` at the repository root. Run clang-format before committing. Key points:

- C++23 standard
- Allman braces
- Indent width: 2 spaces, no tabs
- Column limit: 130
- No trailing whitespace
- Includes are sorted and grouped automatically; do not manually reorder contrary to the formatter
- Pointer alignment: `int* p` (asterisk binds to the name) per formatter rules

## Headers and Type Traits

- Use `TypeTraits.h` for type traits
- Prefer `*_v` variable templates instead of `::value` where possible
- Use `concept` aliases (e.g. `Dereferencable`, `Arrowable`) only in template-heavy code where it improves clarity

## Tests

- Runtime tests use GoogleTest (`EXPECT_*` / `ASSERT_*`)
- Prefer focused small tests over large aggregated ones

## Warnings / Errors

`-Werror` is enabled; code must compile warning-free

## Thread Safety

- Use `ThreadSafe` and `ThreadSafeProxy` patterns already present when guarding shared state

## Documentation

- Keep public headers self-explanatory; avoid redundant comments
- Do not add boilerplate comments where the code is clear
