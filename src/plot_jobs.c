/*
** Copyright 2014-2018 The Earlham Institute
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*
 * plot_jobs.c
 *
 *  Created on: 1 Oct 2018
 *      Author: billy
 */


#include "plot_jobs.h"

#include "plot.h"
#include "time_util.h"
#include "string_utils.h"


static const char * const S_SOWING_TITLE_S = "Sowing date";
static const char * const S_HARVEST_TITLE_S = "Harvest date";
static const char * const S_WIDTH_TITLE_S = "Width";
static const char * const S_LENGTH_TITLE_S = "Length";
static const char * const S_ROW_TITLE_S = "Row";
static const char * const S_COLUMN_TITLE_S = "Column";
static const char * const S_TRIAL_DESIGN_TITLE_S = "Trial design";
static const char * const S_GROWING_CONDITION_TITLE_S = "Growing condition";
static const char * const S_TREATMENT_TITLE_S = "Treatment";

static NamedParameterType S_PLOT_SOWING_DATE = { "PL Sowing Year", PT_TIME };
static NamedParameterType S_PLOT_HARVEST_DATE = { "PL Harvest Year", PT_TIME };
static NamedParameterType S_PLOT_WIDTH = { "PL Width", PT_UNSIGNED_REAL };
static NamedParameterType S_PLOT_LENGTH = { "PL Length", PT_UNSIGNED_REAL };


static NamedParameterType S_PLOT_TRIAL_DESIGN = { "PL Trial Design", PT_STRING };
static NamedParameterType S_PLOT_GROWING_CONDITION = { "PL Growing Condition", PT_STRING };
static NamedParameterType S_PLOT_TREATMENT = { "PL Treatment", PT_STRING };

static NamedParameterType S_PLOT_TABLE_COLUMN_DELIMITER = { "PL Data delimiter", PT_CHAR };
static NamedParameterType S_PLOT_TABLE = { "PL Upload", PT_TABLE};

static const char S_DEFAULT_COLUMN_DELIMITER =  '|';


bool AddPlotParams (ServiceData *data_p, ParameterSet *param_set_p)
{
	bool success_flag = false;
	Parameter *param_p = NULL;
	ParameterGroup *group_p = CreateAndAddParameterGroupToParameterSet ("Plots", NULL, false, data_p, param_set_p);
	SharedType def;

	def.st_char_value = S_DEFAULT_COLUMN_DELIMITER;

	if ((param_p = CreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_TABLE_COLUMN_DELIMITER.npt_type, false, S_PLOT_TABLE_COLUMN_DELIMITER.npt_name_s, "Delimiter", "The character delimiting columns", NULL, def, NULL, NULL, PL_ALL, NULL)) != NULL)
		{
			def.st_string_value_s = NULL;

			if ((param_p = CreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_TABLE.npt_type, false, S_PLOT_TABLE.npt_name_s, "Plot data to upload", "The data to upload", NULL, def, NULL, NULL, PL_ALL, NULL)) != NULL)
				{
					const char delim_s [2] = { S_DEFAULT_COLUMN_DELIMITER, '\0' };
					char *headers_s = ConcatenateVarargsStrings (S_SOWING_TITLE_S, delim_s, S_HARVEST_TITLE_S, delim_s, S_WIDTH_TITLE_S, delim_s, S_LENGTH_TITLE_S, delim_s, S_ROW_TITLE_S, delim_s, S_COLUMN_TITLE_S, delim_s,
																											 S_TRIAL_DESIGN_TITLE_S, delim_s, S_GROWING_CONDITION_TITLE_S, delim_s, S_TREATMENT_TITLE_S, delim_s, NULL);

					if (headers_s)
						{
							if (AddParameterKeyValuePair (param_p, PA_TABLE_COLUMN_HEADINGS_S, headers_s))
								{
									if (AddParameterKeyValuePair (param_p, PA_TABLE_COLUMN_DELIMITER_S, delim_s))
										{
											SharedType date_def;

											InitSharedType (&date_def);

											if ((date_def.st_time_p = AllocateTime ()) != NULL)
												{
													SetDateValuesForTime (date_def.st_time_p, 2017, 1, 1);

													if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_SOWING_DATE.npt_type, S_PLOT_SOWING_DATE.npt_name_s, S_SOWING_TITLE_S, "The date when the seeds were sown", date_def, PL_BASIC)) != NULL)
														{
															if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_HARVEST_DATE.npt_type, S_PLOT_HARVEST_DATE.npt_name_s, S_HARVEST_TITLE_S, "The date when the seeds were harvested", date_def, PL_BASIC)) != NULL)
																{
																	InitSharedType (&def);

																	def.st_ulong_value = 0;

																	if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_WIDTH.npt_type, S_PLOT_WIDTH.npt_name_s, S_WIDTH_TITLE_S, "The width of the plot", def, PL_BASIC)) != NULL)
																		{
																			if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_LENGTH.npt_type, S_PLOT_LENGTH.npt_name_s, S_LENGTH_TITLE_S, "The length of the plot", def, PL_BASIC)) != NULL)
																				{
																					if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_TRIAL_DESIGN.npt_type, S_PLOT_TRIAL_DESIGN.npt_name_s, S_TRIAL_DESIGN_TITLE_S, "The trial desgin of the plot", def, PL_BASIC)) != NULL)
																						{
																							if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_TREATMENT.npt_type, S_PLOT_TREATMENT.npt_name_s, S_TREATMENT_TITLE_S, "The treatments of the plot", def, PL_BASIC)) != NULL)
																								{
																									if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_GROWING_CONDITION.npt_type, S_PLOT_GROWING_CONDITION.npt_name_s, S_GROWING_CONDITION_TITLE_S, "The growing condtions of the plot", def, PL_BASIC)) != NULL)
																										{
																											success_flag = true;
																										}
																								}
																						}
																				}
																		}
																}
														}

													ClearSharedType (&date_def, PT_TIME);
												}		/* if ((sowing_date.st_time_p = AllocateTime ()) != NULL) */

										}		/* if (AddParameterKeyValuePair (param_p, PA_TABLE_COLUMN_DELIMITER_S, delim_s)) */

								}		/* if (AddParameterKeyValuePair (param_p, PA_TABLE_COLUMN_HEADINGS_S, headers_s)) */

							FreeCopiedString (headers_s);
						}		/* if (headers_s) */

				}

		}


	return success_flag;
}


bool RunForPlotParams (DFWFieldTrialServiceData *data_p, ParameterSet *param_set_p, ServiceJob *job_p)
{
	bool job_done_flag = false;

	return job_done_flag;
}
