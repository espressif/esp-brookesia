#!/bin/bash
#
# Check if folders with localized documentation are in sync:
# - Same relative file paths in 'en' and 'zh_CN'
# - Same line count for each paired file
# - For .rst pairs with matching line count: same explicit .. _anchor: names in order
# - Every 'en' .rst has :link_to_translation:`zh_CN:[中文]`
# - Every 'zh_CN' .rst has :link_to_translation:`en:[English]`
# - Every heading in 'en' .rst uses Title Case with preserved technical acronyms
#

set -e

RESULT=0
STARS='***************************************************'

cd "$(dirname "$0")"

# Extract explicit RST hyperlink target names (.. _label:) one per line, in file order.
extract_anchor_labels() {
    grep -E '^\.\.\s+_[^:]+:\s*$' "$1" 2>/dev/null | sed -E 's/^\.\.\s+_([^:]+):\s*$/\1/' || true
}

check_en_title_case() {
    python3 - "$1" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
lines = path.read_text(encoding='utf-8').splitlines()
heading_re = re.compile(r'^[=\-`:\'"~^+*#<>]{3,}$')

whole_exceptions = {
    'ai': 'AI',
    'api': 'API',
    'esp-brookesia': 'ESP-Brookesia',
    'esp': 'ESP',
    'hal': 'HAL',
    'nvs': 'NVS',
    'sntp': 'SNTP',
    'ntp': 'NTP',
    'wi-fi': 'Wi-Fi',
    'openai': 'OpenAI',
    'coze': 'Coze',
    'xiaozhi': 'XiaoZhi',
    'rpc': 'RPC',
    'softap': 'SoftAP',
}

segment_exceptions = {
    'ai': 'AI',
    'api': 'API',
    'esp': 'ESP',
    'hal': 'HAL',
    'nvs': 'NVS',
    'sntp': 'SNTP',
    'ntp': 'NTP',
    'openai': 'OpenAI',
    'coze': 'Coze',
    'xiaozhi': 'XiaoZhi',
    'rpc': 'RPC',
    'softap': 'SoftAP',
}

titlecase_small_words = {
    'a', 'an', 'and', 'as', 'at', 'but', 'by', 'for', 'in', 'nor',
    'of', 'on', 'or', 'the', 'to', 'up', 'via', 'vs'
}

def cap_segment(seg: str) -> str:
    lower = seg.lower()
    if lower in segment_exceptions:
        return segment_exceptions[lower]
    return seg[:1].upper() + seg[1:].lower() if seg else seg

def transform_token(token: str) -> str:
    lower = token.lower()
    if lower in whole_exceptions:
        return whole_exceptions[lower]
    if '/' in token:
        return '/'.join(transform_token(part) for part in token.split('/'))
    if '-' in token:
        return '-'.join(cap_segment(part) for part in token.split('-'))
    match = re.match(r'^(.*?)([A-Za-z][A-Za-z0-9\-/]*)([^A-Za-z]*)$', token)
    if not match:
        return token
    prefix, core, suffix = match.groups()
    lower_core = core.lower()
    if lower_core in whole_exceptions:
        normalized = whole_exceptions[lower_core]
    else:
        normalized = cap_segment(core)
    return prefix + normalized + suffix

for idx in range(len(lines) - 1):
    title = lines[idx]
    underline = lines[idx + 1].strip()
    if not title or not heading_re.fullmatch(underline):
        continue
    raw_tokens = re.split(r'(\s+)', title)
    word_indexes = [i for i, tok in enumerate(raw_tokens) if tok and not tok.isspace()]
    normalized_tokens = []
    for i, tok in enumerate(raw_tokens):
        if not tok or tok.isspace():
            normalized_tokens.append(tok)
            continue

        normalized_tok = transform_token(tok)
        if i in word_indexes[1:-1]:
            match = re.match(r'^([^A-Za-z]*)([A-Za-z][A-Za-z0-9\-/]*)([^A-Za-z]*)$', tok)
            if match:
                prefix, core, suffix = match.groups()
                lower_core = core.lower()
                if lower_core in titlecase_small_words:
                    normalized_tok = prefix + lower_core + suffix

        normalized_tokens.append(normalized_tok)

    normalized = ''.join(normalized_tokens)
    if normalized != title:
        print(f"{idx + 1}:{title} -> {normalized}")
PY
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
TITLE_CASE_MISMATCHES=""
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

        title_case_output="$(check_en_title_case "en/$rel")"
        if [ -n "$title_case_output" ]; then
            TITLE_CASE_MISMATCHES="${TITLE_CASE_MISMATCHES}${STARS}"$'\n'
            TITLE_CASE_MISMATCHES="${TITLE_CASE_MISMATCHES}English heading title-case mismatch: ${rel}"$'\n'
            TITLE_CASE_MISMATCHES="${TITLE_CASE_MISMATCHES}${title_case_output}"$'\n'
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

if ! [ -z "$TITLE_CASE_MISMATCHES" ]; then
    echo "$TITLE_CASE_MISMATCHES"
    echo "$STARS"
    echo "Build failed: headings in 'en' .rst files must use Title Case."
    echo "Preserve technical acronyms and established names such as API, RPC, Wi-Fi, OpenAI, and SoftAP."
    RESULT=1
fi

rm -f file_list_en file_list_zh_CN

exit $RESULT
