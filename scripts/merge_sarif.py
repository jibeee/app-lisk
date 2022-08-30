import glob
import json
import os
import re
import sys
import shutil


def merge_sarif_files(output_dir, sort_files=False):
    """ Reads and merges all .sarif files in the given output directory.
    Each sarif file in the output directory is understood as a single run
    and thus appear separate in the top level runs array. This requires
    modifying the run index of any embedded links in messages.
    """

    def empty(file_name):
        return os.stat(file_name).st_size == 0

    def update_sarif_object(sarif_object, runs_count_offset):
        """
            Given a SARIF object, checks its dictionary entries for a 'message' property.
            If it exists, updates the message index of embedded links in the run index.
            Recursively looks through entries in the dictionary.
        """
        if not isinstance(sarif_object, dict):
            return sarif_object

        if 'message' in sarif_object:
            sarif_object['message'] = match_and_update_run(sarif_object['message'], runs_count_offset)

        for key in sarif_object:
            if isinstance(sarif_object[key], list):
                # iterate through subobjects and update it.
                arr = [update_sarif_object(entry, runs_count_offset) for entry in sarif_object[key]]
                sarif_object[key] = arr
            elif isinstance(sarif_object[key], dict):
                sarif_object[key] = update_sarif_object(sarif_object[key], runs_count_offset)
            else:
                # do nothing
                pass

        return sarif_object


    def match_and_update_run(message, runs_count_offset):
        """
            Given a SARIF message object, checks if the text property contains an embedded link and
            updates the run index if necessary.
        """
        if 'text' not in message:
            return message

        # we only merge runs, so we only need to update the run index
        pattern = re.compile(r'sarif:/runs/(\d+)')

        text = message['text']
        matches = re.finditer(pattern, text)
        matches_list = list(matches)

        # update matches from right to left to make increasing character length (9->10) smoother
        for idx in range(len(matches_list) - 1, -1, -1):
            match = matches_list[idx]
            new_run_count = str(runs_count_offset + int(match.group(1)))
            text = text[0:match.start(1)] + new_run_count + text[match.end(1):]

        message['text'] = text
        return message



    sarif_files = (file for file in glob.iglob(os.path.join(output_dir, '*.sarif')) if not empty(file))
    # exposed for testing since the order of files returned by glob is not guaranteed to be sorted
    if sort_files:
        sarif_files = list(sarif_files)
        sarif_files.sort()

    runs_count = 0
    merged = {}
    for sarif_file in sarif_files:
        with open(sarif_file) as fp:
            sarif = json.load(fp)
            if 'runs' not in sarif:
                continue

            # start with the first file
            if not merged:
                merged = sarif
            else:
                # extract the run and append it to the merged output
                for run in sarif['runs']:
                    new_run = update_sarif_object(run, runs_count)
                    merged['runs'].append(new_run)

            runs_count += len(sarif['runs'])

    with open(os.path.join(output_dir, 'results-merged.sarif'), 'w') as out:
        json.dump(merged, out, indent=4, sort_keys=True)


def main():
    report_dirs = os.listdir(sys.argv[1])
    assert len(report_dirs) == 1
    output_dir = os.path.join(sys.argv[1], report_dirs[0])
    merge_sarif_files(output_dir)
    shutil.copyfile(os.path.join(output_dir, "results-merged.sarif"), "results-merged.sarif")


if __name__ == "__main__":
    main()
