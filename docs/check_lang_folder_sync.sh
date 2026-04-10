#!/bin/bash
#
# Check if folders with localized documentation are in sync:
# - Same relative file paths in 'en' and 'zh_CN'
# - Same line count for each paired file
# - For .rst pairs with matching line count: same explicit .. _anchor: names in order
# - Every 'en' .rst has :link_to_translation:`zh_CN:[中文]`
# - Every 'zh_CN' .rst has :link_to_translation:`en:[English]`
#

set -e

RESULT=0
STARS='***************************************************'

cd "$(dirname "$0")"

# Extract explicit RST hyperlink target names (.. _label:) one per line, in file order.
extract_anchor_labels() {
    grep -E '^\.\.\s+_[^:]+:\s*$' "$1" 2>/dev/null | sed -E 's/^\.\.\s+_([^:]+):\s*$/\1/' || true
}

find en -type f | cut -d/ -f2- | sort > file_list_en
find zh_CN -type f | cut -d/ -f2- | sort > file_list_zh_CN

DIFF_FORMAT="--unchanged-line-format= --old-line-format=[en]:%L --new-line-format=[zh_CN]:%L"

FOLDER_DIFFERENCES=$(diff $DIFF_FORMAT file_list_en file_list_zh_CN || true)
if ! [ -z "$FOLDER_DIFFERENCES" ]; then
    echo "$STARS"
    echo "Build failed due to the following differences in 'en' and 'zh_CN' folders:"
    echo "$FOLDER_DIFFERENCES"
    echo "$STARS"
    echo "Please synchronize contents of 'en' and 'zh_CN' folders to contain files with identical names"
    RESULT=1
fi

LINE_MISMATCHES=""
ANCHOR_MISMATCHES=""
TRANSLATION_LINK_MISMATCHES=""
while IFS= read -r rel; do
    [ -z "$rel" ] && continue
    en_lines=$(wc -l < "en/$rel" | tr -d '[:space:]')
    zh_lines=$(wc -l < "zh_CN/$rel" | tr -d '[:space:]')
    if [ "$en_lines" != "$zh_lines" ]; then
        LINE_MISMATCHES="${LINE_MISMATCHES}[en]:${en_lines} lines  [zh_CN]:${zh_lines} lines  ${rel}"$'\n'
        RESULT=1
        continue
    fi

    case "$rel" in
    *.rst)
        anchor_diff=$(
            diff <(extract_anchor_labels "en/$rel") <(extract_anchor_labels "zh_CN/$rel") || true
        )
        if [ -n "$anchor_diff" ]; then
            ANCHOR_MISMATCHES="${ANCHOR_MISMATCHES}${STARS}"$'\n'
            ANCHOR_MISMATCHES="${ANCHOR_MISMATCHES}Anchor name mismatch (line counts match) for: ${rel}"$'\n'
            ANCHOR_MISMATCHES="${ANCHOR_MISMATCHES}${anchor_diff}"$'\n'
            RESULT=1
        fi

        if ! grep -q "link_to_translation.*zh_CN" "en/$rel" 2>/dev/null; then
            TRANSLATION_LINK_MISMATCHES="${TRANSLATION_LINK_MISMATCHES}  [en] missing :link_to_translation:\`zh_CN:[中文]\`: ${rel}"$'\n'
            RESULT=1
        fi
        if ! grep -q "link_to_translation.*en:" "zh_CN/$rel" 2>/dev/null; then
            TRANSLATION_LINK_MISMATCHES="${TRANSLATION_LINK_MISMATCHES}  [zh_CN] missing :link_to_translation:\`en:[English]\`: ${rel}"$'\n'
            RESULT=1
        fi
        ;;
    esac
done < <(comm -12 file_list_en file_list_zh_CN)

if ! [ -z "$LINE_MISMATCHES" ]; then
    echo "$STARS"
    echo "Build failed: line count mismatch between 'en' and 'zh_CN' for the following files:"
    echo -n "$LINE_MISMATCHES"
    echo "$STARS"
    echo "Please align line counts (e.g. keep translations structurally parallel)."
    RESULT=1
fi

if ! [ -z "$ANCHOR_MISMATCHES" ]; then
    echo "$ANCHOR_MISMATCHES"
    echo "$STARS"
    echo "Build failed: explicit '.. _anchor:' labels differ between 'en' and 'zh_CN' (same line count)."
    echo "Keep the same anchor names and order in paired .rst files."
    RESULT=1
fi

if ! [ -z "$TRANSLATION_LINK_MISMATCHES" ]; then
    echo "$STARS"
    echo "Build failed: missing :link_to_translation: directives in the following .rst files:"
    echo -n "$TRANSLATION_LINK_MISMATCHES"
    echo "$STARS"
    echo "Each 'en' .rst must contain :link_to_translation:\`zh_CN:[中文]\`"
    echo "Each 'zh_CN' .rst must contain :link_to_translation:\`en:[English]\`"
    RESULT=1
fi

rm -f file_list_en file_list_zh_CN

exit $RESULT
