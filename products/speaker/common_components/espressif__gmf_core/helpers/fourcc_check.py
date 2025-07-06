import re
from collections import Counter
import sys

def extract_fourcc_definitions(file_path):
    """
    Extract FOURCC characters from a header file.
    Args:
        file_path (str): Path to the header file.
    Returns:
        list: A list of tuples containing FOURCC strings and their corresponding lines.
    """
    fourcc_pattern = re.compile(r"ESP_FOURCC_TO_INT\('(.{1})', '(.{1})', '(.{1})', '(.{1})'\)")
    fourcc_list = []

    with open(file_path, 'r') as f:
        for line_number, line in enumerate(f, 1):
            match = fourcc_pattern.search(line)
            if match:
                fourcc = ''.join(match.groups())  # Combine the matched characters
                fourcc_list.append((fourcc, line_number, line.strip()))

    return fourcc_list

def check_duplicates(fourcc_list):
    """
    Check for duplicates in the FOURCC list.
    Args:
        fourcc_list (list): A list of tuples containing FOURCC strings and their corresponding lines.
    Returns:
        dict: A dictionary with duplicate FOURCC strings and their occurrences (line numbers and lines).
    """
    counter = Counter([fourcc for fourcc, _, _ in fourcc_list])
    duplicates = {
        fourcc: [(line_number, line) for fcc, line_number, line in fourcc_list if fcc == fourcc]
        for fourcc, count in counter.items() if count > 1
    }
    return duplicates

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python fourcc_check.py <header_file_path>')
        sys.exit(1)
    header_file_path = sys.argv[1]
    try:
        fourcc_list = extract_fourcc_definitions(header_file_path)
        duplicates = check_duplicates(fourcc_list)

        if duplicates:
            print('Duplicate FOURCC definitions found:')
            for fourcc, occurrences in duplicates.items():
                print(f'{fourcc}: {len(occurrences)} times')
                for line_number, line in occurrences:
                    print(f'  Line {line_number}: {line}')
        else:
            print('No duplicate FOURCC definitions found.')
    except FileNotFoundError:
        print(f'Error: File not found: {header_file_path}')
        sys.exit(1)
    except Exception as e:
        print(f'An error occurred: {e}')
        sys.exit(1)
