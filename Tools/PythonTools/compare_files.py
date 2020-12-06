# Function for comparing two files.
import os
import argparse
import collections
import copy
import difflib
import json
import logging
import numpy as np
import pandas as pd
from datacompy.core import Compare
import re


def has_csv_config(config, filename_1, filename_2):
    # Check if config has a configuration for filename_1 or filename_2

    for filename in [filename_1, filename_2]:
        for key in config['csv_settings']['files']:
            if re.match(key, filename):
                return config['csv_settings']['files'][key]

    return None


def create_df(file, col_types=None):
    # Read csv or json file into a Dataframe.

    logger = logging.getLogger(__name__)

    logger.debug('Start creating DataFrame from file %s.', file)

    _, filename = os.path.split(file)
    _, file_extension = os.path.splitext(file)

    if file_extension == '.csv' or file_extension == '.txt':
        logger.debug('Creating DataFrame from csv file %s.', file)
        return pd.read_csv(file, dtype=col_types)
    elif file_extension == '.json':
        if filename == 'simm.json':
            with open(file, 'r') as json_file:
                simm_json = json.load(json_file)
                if 'simmReport' in simm_json:

                    logger.debug('Creating DataFrame from simm json file %s.', file)
                    simm_df = pd.DataFrame(simm_json['simmReport'])

                    # In a lot of regression tests, the NettingSetId field was left blank which in the simm JSON
                    # response is an empty string. This empty string is then in the portfolio column in the DataFrame
                    # created from the simm JSON. When the simm csv file is read in to a DataFrame, the empty field
                    # under Portfolio is read in to a DataFrame as Nan. We replace the empty string here with Nan in
                    # portfolio column so that everything works downstream.
                    simm_df['portfolio'].replace('', np.nan, inplace=True)

                    return simm_df

                else:
                    logger.warning('Expected simm json file %s to contain the field simmReport.', file)
                    return None
    else:
        logger.warning('File %s is neither a csv nor a json file so cannot create DataFrame.', file)
        return None

    logger.debug('Finished creating DataFrame from file %s.', file)


def compare_files(file_1, file_2, name, config=None):
    logger = logging.getLogger(__name__)

    logger.info('%s: Start comparing file %s against %s', name, file_1, file_2)

    # Check that both file paths actually exist.
    for file in [file_1, file_2]:
        if not os.path.isfile(file):
            logger.warning('The file path %s does not exist.', file)
            return False

    # Attempt to get the csv comparison configuration to use for the given filenames.
    csv_comp_config = None
    if config is not None:
        _, filename_1 = os.path.split(file_1)
        _, filename_2 = os.path.split(file_2)
        csv_comp_config = has_csv_config(config, filename_1, filename_2)

    # If the file extension is csv or json, enforce the use of a comparison configuration.
    # If the file is a txt file, attempt to find a comparison config. If none, proceed with direct comparison.
    _, ext_1 = os.path.splitext(file_1)
    _, ext_2 = os.path.splitext(file_2)

    if csv_comp_config is None:
        if ext_1 == '.csv' or ext_1 == '.json':
            raise ValueError('File, ' + file_1 + ', requires a comparison configuration but none given.')
        if ext_2 == '.csv' or ext_2 == '.json':
            raise ValueError('File, ' + file_2 + ', requires a comparison configuration but none given.')

    if csv_comp_config is None:
        # If there was no configuration then fall back to a straight file comparison.
        result = compare_files_direct(name, file_1, file_2)
    else:
        # If there was a configuration, use it for the comparison.
        result = compare_files_df(name, file_1, file_2, csv_comp_config)

    logger.info('%s: Finished comparing file %s against %s: %s.', name, file_1, file_2, result)

    return result


def compare_files_df(name, file_1, file_2, config):
    # Compare files using dataframes and a configuration.

    logger = logging.getLogger(__name__)

    logger.debug('%s: Start comparing file %s against %s using configuration.', name, file_1, file_2)

    # We can force the type of specific columns here.
    col_types = None
    if 'col_types' in config:
        col_types = config['col_types']

    # Read the files in to dataframes
    df_1 = create_df(file_1, col_types)
    df_2 = create_df(file_2, col_types)

    # Check that a DataFrame could be created for each.
    if df_1 is None:
        logger.warning('A DataFrame could not be created from the file %s.', file_1)
        return False

    if df_2 is None:
        logger.warning('A DataFrame could not be created from the file %s.', file_2)
        return False

    # In most cases, the header column in our csv files starts with a #. Remove it here if necessary.
    for df in [df_1, df_2]:
        first_col_name = df.columns[0]
        if first_col_name.startswith('#'):
            df.rename(columns={first_col_name: first_col_name[1:]}, inplace=True)

    # If we are asked to rename columns, try to do it here.
    if 'rename_cols' in config:
        logger.debug('Applying column renaming, %s, to both DataFrames', str(config['rename_cols']))
        for idx, df in enumerate([df_1, df_2]):
            df.rename(columns=config['rename_cols'], inplace=True)

    # Possibly drop some rows where specified column values are not above the threshold.
    if 'drop_rows' in config:

        criteria_cols = config['drop_rows']['cols']
        threshold = config['drop_rows']['threshold']

        for idx, df in enumerate([df_1, df_2]):
            if not all([elem in df.columns for elem in criteria_cols]):
                logger.warning('The columns, %s, in Dataframe %d do not contain all the drop_rows columns, %s.',
                               str(list(df.columns.values)), idx + 1, str(criteria_cols))
                return False

        df_1 = df_1[pd.DataFrame(abs(df_1[criteria_cols]) > threshold).all(axis=1)]
        df_2 = df_2[pd.DataFrame(abs(df_2[criteria_cols]) > threshold).all(axis=1)]

    # We must know the key(s) on which the comparison is to be performed.
    if 'keys' not in config:
        logger.warning('The comparison configuration must contain a keys field.')
        return False

    # Get the keys and do some checks.
    keys = config['keys']

    # Check keys are not empty
    if not keys or any([elem == '' for elem in keys]):
        logger.warning('The list of keys, %s, must be non-empty and each key must be a non-empty string.', str(keys))
        return False

    # Check that keys contain no duplicates
    dup_keys = [elem for elem, count in collections.Counter(keys).items() if count > 1]
    if dup_keys:
        logger.warning('The keys, %s, contain duplicates, %s.', str(keys), str(dup_keys))
        return False

    # Check that all keys are in each DataFrame
    for idx, df in enumerate([df_1, df_2]):
        if not all([elem in df.columns for elem in keys]):
            logger.warning('The columns, %s, in Dataframe %d do not contain all the keys, %s.',
                           str(list(df.columns.values)), idx + 1, str(keys))
            return False

    # If we are told to use only certain columns, drop the others in each DataFrame. We first check that both
    # DataFrames have all of the explicitly listed columns to use.
    if 'use_cols' in config:

        use_cols = copy.deepcopy(config['use_cols'])
        logger.debug('We will only use the columns, %s, in the comparison.', str(use_cols))
        use_cols += keys

        # Check that all of the requested columns are in the DataFrames.
        for idx, df in enumerate([df_1, df_2]):
            if not all([elem in df.columns for elem in use_cols]):
                logger.warning('The columns, %s, in Dataframe %d do not contain all the named columns, %s.',
                               str(list(df.columns.values)), idx + 1, str(use_cols))
                return False

        # Use only the requested columns.
        df_1 = df_1[use_cols]
        df_2 = df_2[use_cols]

    # Flag that will store the ultimate result i.e. True if the files are considered a match and False otherwise.
    is_match = True

    # Certain groups of columns may need special tolerances for their comparison. Deal with them first.
    cols_compared = set()
    if 'column_settings' in config:

        for col_group_config in config['column_settings']:

            names = col_group_config['names'].copy()

            # If there are optional names, add them to compare if they are in both dataframes. If in one but not
            # another, mark the match as false.
            if 'optional_names' in col_group_config:
                optional_names = col_group_config['optional_names']
                for optional_name in optional_names:
                    if optional_name in df_1.columns and optional_name in df_2.columns:
                        names.append(optional_name)
                    elif optional_name not in df_1.columns and optional_name in df_2.columns:
                        logger.warning('The optional name, %s, is in df_1 but not in df_2.', optional_name)
                        logger.info('Skipping comparison for this group of names and marking files as different.')
                        is_match = False
                        continue
                    elif optional_name in df_1.columns and optional_name not in df_2.columns:
                        logger.warning('The optional name, %s, is in df_2 but not in df_1.', optional_name)
                        logger.info('Skipping comparison for this group of names and marking files as different.')
                        is_match = False
                        continue

            if not names:
                logger.debug('No column names provided. Use joint columns from both files except keys')
                names = [s for s in list(set(list(df_1.columns) + list(df_2.columns))) if s not in keys]

            logger.info('Performing comparison of files for column names: %s.', str(names))

            # Check that keys contain no duplicates
            dup_names = [elem for elem, count in collections.Counter(names).items() if count > 1]
            if dup_names:
                logger.warning('The names, %s, contain duplicates, %s.', str(names), str(dup_names))
                logger.info('Skipping comparison for this group of names and marking files as different.')
                is_match = False
                continue

            # Check that none of the keys appear in names.
            if any([elem in keys for elem in names]):
                logger.warning('The names, %s, contain some of the keys, %s.', str(names), str(keys))
                logger.info('Skipping comparison for this group of names and marking files as different.')
                is_match = False
                continue

            # Check that all of the names are in each DataFrame
            all_names = True
            for idx, df in enumerate([df_1, df_2]):
                if not all([elem in df.columns for elem in names]):
                    logger.warning('The column names, %s, in Dataframe %d do not contain all the names, %s.',
                                   str(list(df.columns.values)), idx + 1, str(names))
                    all_names = False
                    break

            if not all_names:
                logger.info('Skipping comparison for this group of names and marking files as different.')
                is_match = False
                continue

            # We will compare this subset of columns using the provided tolerances.
            col_names = keys + names

            # Add to the columns that we have already compared.
            cols_compared.update(names)

            abs_tol = 0.0
            if 'abs_tol' in col_group_config and col_group_config['abs_tol'] is not None:
                abs_tol = col_group_config['abs_tol']

            rel_tol = 0.0
            if 'rel_tol' in col_group_config and col_group_config['rel_tol'] is not None:
                rel_tol = col_group_config['rel_tol']

            sub_df_1 = df_1[col_names]
            sub_df_2 = df_2[col_names]

            comp = Compare(sub_df_1, sub_df_2, join_columns=keys, abs_tol=abs_tol, rel_tol=rel_tol,
                           df1_name='expected', df2_name='calculated')

            if comp.matches():
                logger.debug('The columns, %s, in the files match.', str(names))
            else:
                logger.debug('The columns, %s, in the files do not match.', str(names))
                is_match = False
                logger.info(comp.report())

    # Get the remaining columns that have not been compared.
    rem_cols_1 = [col for col in df_1.columns if col not in cols_compared]
    rem_cols_2 = [col for col in df_2.columns if col not in cols_compared]
    sub_df_1 = df_1[rem_cols_1]
    sub_df_2 = df_2[rem_cols_2]
    logger.debug('The remaining columns in the first file are: %s.', str(rem_cols_1))
    logger.debug('The remaining columns in the second file are: %s.', str(rem_cols_2))

    comp = Compare(sub_df_1, sub_df_2, join_columns=keys, df1_name='expected', df2_name='calculated')

    if comp.all_columns_match() and comp.matches():
        logger.debug('The remaining columns in the files match.', )
    else:
        logger.info('The remaining columns in the files do not match:')
        is_match = False
        logger.info(comp.report())

    logger.debug('%s: Finished comparing file %s against %s using configuration: %s.', name, file_1, file_2, is_match)

    return is_match


def compare_files_direct(name, file_1, file_2):
    # Check that the contents of the two files are identical.

    logger = logging.getLogger(__name__)

    logger.debug('%s: Comparing file %s directly against %s', name, file_1, file_2)

    with open(file_1, 'r') as f1, open(file_2, 'r') as f2:
        diff = difflib.unified_diff(f1.readlines(), f2.readlines(), fromfile=file_1, tofile=file_2)
        match = True
        for line in diff:
            match = False
            logger.warning(line.rstrip('\n'))

    return match


if __name__ == "__main__":
    # Parse input parameters
    parser = argparse.ArgumentParser(description='Compare two input files')
    parser.add_argument('--file_1', help='First file in comparison', required=True)
    parser.add_argument('--file_2', help='Second file in comparison', required=True)
    parser.add_argument('--config', help='Path to comparison configuration file', default='comparison_config.json')
    args = parser.parse_args()

    # Read in the comparison configuration
    with open(args.config, 'r') as f:
        comparison_config = json.load(f)

    main_logger = logging.getLogger(__name__)
    main_logger.info('Start comparison of files.')
    main_result = compare_files(args.file_1, args.file_2, "test", comparison_config)
    main_logger.info('Finished comparison of files: %s.', main_result)
