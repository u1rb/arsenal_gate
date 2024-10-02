

```bash
# Parse --cell parameter
CELLS=""

for arg in "$@"; do
    case $arg in
        --cell=*)
            CELLS="${arg#*=}"
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            echo "Error: Unknown argument '$arg'"
            echo ""
            show_help
            ;;
    esac
done

# Show help if cells is empty
if [ -z "$CELLS" ]; then
    echo "Error: --cell parameter is required, such as $0 --cell=1,2,3"
    echo ""
    show_help
fi


# Helper function to check if action is in cells
has_cell() {
    [[ ",$CELLS," == *",$1,"* ]]
}
```
Usage: 
```bash

# Clean build directory
if has_cell "clean"; then
    print_status "Cleaning build directory..."
    rm -rf build
fi
```
