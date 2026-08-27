#!/bin/sh
set -eu

output=$1
entry=$2
shift 2

compiler=${CLANG:-clang}

object_dir="${output}.objects"
mkdir -p "$object_dir" "$(dirname "$output")"
objects=""

for source in "$@"; do
    object_name=$(printf '%s' "$source" | tr '/.' '__')
    object="$object_dir/$object_name.o"
    "$compiler" --target=loongarch64-unknown-elf ${CPPFLAGS:-} ${USER_CFLAGS:-} \
        ${EXTRA_CFLAGS:-} -c "$source" -o "$object"
    objects="$objects $object"
done

entry_flags=""
if [ "$entry" != "-" ]; then
    entry_flags="-e $entry"
fi

# shellcheck disable=SC2086
ld.lld -m elf64loongarch -shared -Bsymbolic $entry_flags $objects -o "$output"
