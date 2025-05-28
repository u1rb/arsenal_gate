# Clang-Format Integration

## Overview

Added automatic code formatting using `clang-format` to the `run.sh` script for consistent C/C++ code style across the project.

## Usage

```bash
# Format all C/C++ files in the cxx_examples directory
bash cxx_examples/run.sh --cell=format
```

## Features

### **Automatic Code Formatting**
- Formats all `.c`, `.cpp`, `.h`, and `.hpp` files
- Uses a custom `.clang-format` configuration file
- Excludes build directories and dependencies
- Provides clear progress output

### **Smart File Discovery**
```bash
find cxx_examples -name "build" -prune -o -name "_deps" -prune -o \
  \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print
```

**Benefits:**
- **Excludes build directories**: Doesn't format generated or third-party code
- **Includes all source files**: Covers C, C++, and header files
- **Efficient**: Only processes relevant files

### **Error Handling**
- Checks if `clang-format` is installed
- Provides installation instructions for different platforms
- Graceful failure with helpful error messages

## Configuration

### **`.clang-format` Settings**
```yaml
BasedOnStyle: LLVM
IndentWidth: 2
ColumnLimit: 80
BreakBeforeBraces: Attach
PointerAlignment: Right
```

**Key Features:**
- **LLVM-based style**: Industry standard formatting
- **2-space indentation**: Consistent with existing code
- **80-column limit**: Good readability on all screens
- **Attached braces**: Compact, readable style
- **Right pointer alignment**: `char *ptr` style

### **Complete Configuration**
The `.clang-format` file includes settings for:
- Basic formatting (indentation, column limits)
- Brace placement and spacing
- Alignment and line breaks
- Pointer/reference alignment
- Comment formatting
- Include sorting
- Function parameter formatting

## Installation

### **Ubuntu/Debian**
```bash
sudo apt install clang-format
```

### **macOS**
```bash
brew install clang-format
```

### **Other Systems**
- **Fedora/RHEL**: `sudo dnf install clang-tools-extra`
- **Arch Linux**: `sudo pacman -S clang`
- **Windows**: Install via LLVM or Visual Studio

## Integration with Development Workflow

### **Pre-commit Formatting**
```bash
# Format before committing
bash cxx_examples/run.sh --cell=format
git add .
git commit -m "Apply clang-format"
```

### **Combined with Build**
```bash
# Format and build in one command
bash cxx_examples/run.sh --cell=format,build
```

### **IDE Integration**
Most IDEs can use the `.clang-format` file automatically:
- **VS Code**: Install "C/C++" extension
- **CLion**: Built-in support
- **Vim/Neovim**: Use `vim-clang-format` plugin
- **Emacs**: Use `clang-format.el`

## Files Formatted

The format command processes these file types:
- **Source files**: `*.c`, `*.cpp`
- **Header files**: `*.h`, `*.hpp`
- **Library files**: Including `parquet_reader_stream.h`
- **Example files**: All demo and test programs

### **Example Output**
```
Formatting C/C++ files with clang-format...
Formatting: cxx_examples/d_read_stream.c
Formatting: cxx_examples/simple_read_example.c
Formatting: cxx_examples/parquet_reader_stream.h
Formatting: cxx_examples/write_stream.c
...
Formatting completed!
```

## Benefits

### **Code Quality**
- **Consistent style**: All code follows the same formatting rules
- **Improved readability**: Standardized indentation and spacing
- **Reduced diffs**: Formatting changes don't clutter git history
- **Professional appearance**: Clean, well-formatted code

### **Developer Experience**
- **Easy to use**: Single command formats entire codebase
- **Fast execution**: Only processes source files, skips build artifacts
- **Clear feedback**: Shows which files are being formatted
- **Error prevention**: Catches formatting issues early

### **Maintenance**
- **Automated**: No manual formatting needed
- **Configurable**: Easy to adjust style preferences
- **Portable**: Works across different development environments
- **Scalable**: Handles any number of files efficiently

## Best Practices

### **Regular Formatting**
- Run `--cell=format` before committing changes
- Include formatting in CI/CD pipelines
- Format after major refactoring

### **Team Workflow**
- All team members should use the same `.clang-format` file
- Format code before code reviews
- Include formatting checks in pull request workflows

### **Configuration Management**
- Keep `.clang-format` in version control
- Document any style changes in commit messages
- Test formatting changes on sample code first

## Troubleshooting

### **Common Issues**

**clang-format not found:**
```bash
Error: clang-format not found. Please install clang-format.
On Ubuntu/Debian: sudo apt install clang-format
On macOS: brew install clang-format
```

**Permission errors:**
- Ensure write permissions on source files
- Check file ownership and permissions

**Unexpected formatting:**
- Review `.clang-format` configuration
- Test with `clang-format --dry-run` first
- Check clang-format version compatibility

### **Version Compatibility**
- Tested with clang-format 10.0+
- Most settings work with older versions
- Some advanced features require newer versions

## Future Enhancements

### **Potential Improvements**
- **Pre-commit hooks**: Automatic formatting on git commit
- **CI integration**: Format checking in continuous integration
- **Editor integration**: Real-time formatting in development
- **Custom rules**: Project-specific formatting extensions

### **Advanced Features**
- **Selective formatting**: Format only changed files
- **Style validation**: Check formatting without modifying files
- **Multiple configurations**: Different styles for different components
- **Integration with linters**: Combine with static analysis tools

## Conclusion

The clang-format integration provides a robust, automated solution for maintaining consistent code style across the Parquet FFI project. It's easy to use, well-configured, and integrates seamlessly with the existing build system. 