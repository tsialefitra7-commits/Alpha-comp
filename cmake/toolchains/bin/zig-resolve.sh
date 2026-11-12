#!/bin/sh
# Trouve le binaire zig : PATH d'abord, sinon le module pip ziglang.
if command -v zig >/dev/null 2>&1; then
    exec zig "$@"
fi
ZIG=$(python3 -c "import ziglang, os; print(os.path.join(os.path.dirname(ziglang.__file__), 'zig'))" 2>/dev/null)
if [ -n "$ZIG" ] && [ -x "$ZIG" ]; then
    exec "$ZIG" "$@"
fi
echo "zig introuvable : installez 'pip install ziglang' ou ajoutez zig au PATH" >&2
exit 1
