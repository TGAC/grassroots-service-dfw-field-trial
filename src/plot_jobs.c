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
#include "study_jobs.h"
#include "math_utils.h"
#include "material.h"
#include "row.h"
#include "gene_bank.h"
#include "dfw_util.h"

#include "char_parameter.h"
#include "json_parameter.h"
#include "string_parameter.h"



typedef enum
{
	PP_SOWING_DATE,
	PP_HARVEST_DATE,
	PP_WIDTH,
	PP_LENGTH,
	PP_INDEX,
	PP_ROW,
	PP_COLUMN,
	PP_RACK_INDEX,
	PP_MATERIAL,
	PP_TRIAL_DESIGN,
	PP_GROWING_CONDITION,
	PP_TREATMENT,
	PP_NUM_PARAMS
} PlotParam;


static const char * const S_SOWING_TITLE_S = "Sowing date";
static const char * const S_HARVEST_TITLE_S = "Harvest date";
static const char * const S_WIDTH_TITLE_S = "Width";
static const char * const S_LENGTH_TITLE_S = "Length";
static const char * const S_INDEX_TITLE_S = "Plot ID";
static const char * const S_ROW_TITLE_S = "Row";
static const char * const S_COLUMN_TITLE_S = "Column";
static const char * const S_RACK_TITLE_S = "Rack";
static const char * const S_ACCESSION_TITLE_S = "Accession";
static const char * const S_GENE_BANK_S = "Gene Bank";
static const char * const S_TREATMENT_TITLE_S = "Treatment";
static const char * const S_REPLICATE_TITLE_S = "Replicate";
static const char * const S_COMMENT_TITLE_S = "Comment";


static NamedParameterType S_PLOT_TABLE_COLUMN_DELIMITER = { "PL Data delimiter", PT_CHAR };
static NamedParameterType S_PLOT_TABLE = { "PL Upload", PT_JSON_TABLE};


static NamedParameterType S_STUDIES_LIST = { "PL Study", PT_STRING };


static const char S_DEFAULT_COLUMN_DELIMITER =  '|';


/*
 * static declarations
 */



static bool AddPlotsFromJSON (ServiceJob *job_p, const json_t *plots_json_p, Study *study_p,  const DFWFieldTrialServiceData *data_p);

static Parameter *GetTableParameter (ParameterSet *param_set_p, ParameterGroup *group_p, Study *active_study_p, const DFWFieldTrialServiceData *data_p);

static json_t *GetTableParameterHints (void);

static Plot *GetUniquePlot (bson_t *query_p, Study *study_p, const DFWFieldTrialServiceData *data_p);

static json_t *GetPlotTableRow (const Row *row_p, const DFWFieldTrialServiceData *service_data_p);

static json_t *GetStudyPlotsForSubmissionTable (Study *study_p, const DFWFieldTrialServiceData *service_data_p);

static bool AddPlotRowsToTable (const Plot *plot_p, json_t *plots_table_p, const DFWFieldTrialServiceData *service_data_p);

static Plot *CreatePlotFromTabularJSON (const json_t *table_row_json_p, const int32 row, const int32 column, Study *study_p, const DFWFieldTrialServiceData *data_p);

static json_t *GetPlotRowTemplate (const uint32 row, const uint32 column, const double64 *width_p, const double64 *height_p);

static json_t *GeneratePlotsTemplate (const Study *study_p);

static bool AddErrorForTableCell (ServiceJob *job_p, const json_t *table_row_p, const uint32 row, const char *column_key_s, const char *message_s);


/*
 * API definitions
 */

bool AddSubmissionPlotParams (ServiceData *data_p, ParameterSet *param_set_p, Resource *resource_p)
{
	DFWFieldTrialServiceData *dfw_data_p = (DFWFieldTrialServiceData *) data_p;
	bool success_flag = false;
	Parameter *param_p = NULL;
	ParameterGroup *group_p = CreateAndAddParameterGroupToParameterSet ("Plots", false, data_p, param_set_p);
	Study *active_study_p = GetStudyFromResource (resource_p, S_STUDIES_LIST, dfw_data_p);


	if ((param_p = EasyCreateAndAddStringParameterToParameterSet (data_p, param_set_p, group_p, S_STUDIES_LIST.npt_type, S_STUDIES_LIST.npt_name_s, "Study", "The Study that these plots are from", NULL, PL_ALL)) != NULL)
		{
			if (SetUpStudiesListParameter (dfw_data_p, (StringParameter *) param_p, active_study_p, false))
				{
					char c = S_DEFAULT_COLUMN_DELIMITER;

					/*
					 * We want to update all of the values in the form
					 * when a user selects a study from the list so
					 * we need to make the parameter automatically
					 * refresh the values. So we set the
					 * pa_refresh_service_flag to true.
					 */
					param_p -> pa_refresh_service_flag = true;

					if ((param_p = EasyCreateAndAddCharParameterToParameterSet (data_p, param_set_p, group_p, S_PLOT_TABLE_COLUMN_DELIMITER.npt_name_s, "Delimiter", "The character delimiting columns", &c, PL_ADVANCED)) != NULL)
						{

							if ((param_p = GetTableParameter (param_set_p, group_p, active_study_p, dfw_data_p)) != NULL)
								{
									success_flag = true;
								}
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "GetTableParameter failed");
								}
						}
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add %s parameter", S_PLOT_TABLE_COLUMN_DELIMITER.npt_name_s);
						}
				}
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "SetUpStudiesListParameter failed");
				}
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add %s parameter", S_STUDIES_LIST.npt_name_s);
		}

	if (active_study_p)
		{
			FreeStudy (active_study_p);
		}

	return success_flag;
}


bool RunForSubmissionPlotParams (DFWFieldTrialServiceData *data_p, ParameterSet *param_set_p, ServiceJob *job_p)
{
	bool job_done_flag = false;
	const char *study_id_s = NULL;

	if (GetCurrentStringParameterValueFromParameterSet (param_set_p, S_STUDIES_LIST.npt_name_s, &study_id_s))
		{
			Study *study_p = GetStudyByIdString (study_id_s, VF_STORAGE, data_p);

			if (study_p)
				{
					const json_t *plots_table_p = NULL;

					if (GetCurrentJSONParameterValueFromParameterSet (param_set_p, S_PLOT_TABLE.npt_name_s, &plots_table_p))
						{
							job_done_flag = true;

							/*
							 * Has a spreadsheet been uploaded?
							 */
							if (plots_table_p)
								{
									const size_t num_rows = json_array_size (plots_table_p);

									if (num_rows > 0)
										{
											if (!AddPlotsFromJSON (job_p, plots_table_p, study_p, data_p))
												{
													PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, plots_table_p, "AddPlotsFromJSON failed for study \"%s\"", study_p -> st_name_s);
												}

										}		/* if (num_rows > 0) */
									else
										{
											PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Empty JSON for uploaded plots for study \"%s\"", study_id_s);
										}

								}		/* if (table_value.st_json_p) */
							else
								{
									PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "NULL JSON for uploaded plots for study \"%s\"", study_id_s);
								}

						}		/* if (GetCurrentParameterValueFromParameterSet (param_set_p, S_PLOT_TABLE.npt_name_s, &table_value)) */
					else
						{
							PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to get param \%s\"", S_PLOT_TABLE.npt_name_s);
						}

					FreeStudy (study_p);
				}		/* if (study_p) */
			else
				{
					PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to get parent study for plots with id \%s\"", study_id_s);
				}

		}		/* if (GetCurrentParameterValueFromParameterSet (param_set_p, S_STUDIES_LIST.npt_name_s, &parent_study_value)) */
	else
		{
			PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to get param \%s\"", S_STUDIES_LIST.npt_name_s);
		}

	return job_done_flag;
}



bool GetSubmissionPlotParameterTypeForNamedParameter (const char *param_name_s, ParameterType *pt_p)
{
	bool success_flag = true;

	if (strcmp (param_name_s, S_STUDIES_LIST.npt_name_s) == 0)
		{
			*pt_p = S_STUDIES_LIST.npt_type;
		}
	else if (strcmp (param_name_s, S_PLOT_TABLE_COLUMN_DELIMITER.npt_name_s) == 0)
		{
			*pt_p = S_PLOT_TABLE_COLUMN_DELIMITER.npt_type;
		}
	else if (strcmp (param_name_s, S_PLOT_TABLE.npt_name_s) == 0)
		{
			*pt_p = S_PLOT_TABLE.npt_type;
		}
	else
		{
			success_flag = false;
		}

	return success_flag;
}



/*
 * static definitions
 */


static json_t *GetTableParameterHints (void)
{
	/*
	headers_s = ConcatenateVarargsStrings (S_SOWING_TITLE_S, delim_s, S_HARVEST_TITLE_S, delim_s, S_WIDTH_TITLE_S, delim_s, S_LENGTH_TITLE_S, delim_s, S_ROW_TITLE_S, delim_s, S_COLUMN_TITLE_S, delim_s,
																				 S_REPLICATE_TITLE_S, delim_s, S_RACK_TITLE_S, delim_s, S_MATERIAL_TITLE_S, delim_s, S_TRIAL_DESIGN_TITLE_S, delim_s, S_GROWING_CONDITION_TITLE_S, delim_s, S_TREATMENT_TITLE_S, delim_s, NULL);
	 */
	json_t *hints_p = json_array ();

	if (hints_p)
		{
			if (AddColumnParameterHint (S_SOWING_TITLE_S, PT_TIME, hints_p))
				{
					if (AddColumnParameterHint (S_HARVEST_TITLE_S, PT_TIME, hints_p))
						{
							if (AddColumnParameterHint (S_WIDTH_TITLE_S, PT_UNSIGNED_REAL, hints_p))
								{
									if (AddColumnParameterHint (S_LENGTH_TITLE_S, PT_UNSIGNED_REAL, hints_p))
										{
											if (AddColumnParameterHint (S_INDEX_TITLE_S, PT_UNSIGNED_INT, hints_p))
												{
													if (AddColumnParameterHint (S_ROW_TITLE_S, PT_UNSIGNED_INT, hints_p))
														{
															if (AddColumnParameterHint (S_COLUMN_TITLE_S, PT_UNSIGNED_INT, hints_p))
																{
																	if (AddColumnParameterHint (S_REPLICATE_TITLE_S, PT_UNSIGNED_INT, hints_p))
																		{
																			if (AddColumnParameterHint (S_RACK_TITLE_S, PT_UNSIGNED_INT, hints_p))
																				{
																					if (AddColumnParameterHint (S_ACCESSION_TITLE_S, PT_STRING, hints_p))
																						{
																							if (AddColumnParameterHint (S_COMMENT_TITLE_S, PT_STRING, hints_p))
																								{
																									return hints_p;
																								}
																						}
																				}
																		}
																}
														}
												}
										}

								}
						}
				}

			json_decref (hints_p);
		}

	return NULL;
}



static Parameter *GetTableParameter (ParameterSet *param_set_p, ParameterGroup *group_p, Study *active_study_p, const DFWFieldTrialServiceData *data_p)
{
	Parameter *param_p = NULL;
	const char delim_s [2] = { S_DEFAULT_COLUMN_DELIMITER, '\0' };
	bool success_flag = false;
	json_t *hints_p = GetTableParameterHints ();

	if (hints_p)
		{
			json_t *plots_json_p = NULL;

			if (active_study_p)
				{
					plots_json_p = GetStudyPlotsForSubmissionTable (active_study_p, data_p);

					if (plots_json_p)
						{
							success_flag = true;
						}
					else
						{
							/*
							 * Are there default values for the study
							 */
						}
				}
			else
				{
					success_flag = true;
				}

			if (success_flag)
				{
					param_p = EasyCreateAndAddJSONParameterToParameterSet (& (data_p -> dftsd_base_data), param_set_p, group_p, S_PLOT_TABLE.npt_type, S_PLOT_TABLE.npt_name_s, "Plot data to upload", "The data to upload", plots_json_p, PL_ALL);

					if (param_p)
						{
							success_flag = false;

							if (AddParameterKeyJSONValuePair (param_p, PA_TABLE_COLUMN_HEADINGS_S, hints_p))
								{
									if (AddParameterKeyStringValuePair (param_p, PA_TABLE_COLUMN_DELIMITER_S, delim_s))
										{
											success_flag = true;
										}
								}

							if (!success_flag)
								{
									FreeParameter (param_p);
									param_p = NULL;
								}

						}		/* if (param_p) */

				}		/* if (success_flag) */

			json_decref (hints_p);
		}		/* if (hints_p) */


	return param_p;
}


static bool GetIntegerValue (const json_t *data_p, const char *key_s, int32 *value_p, ServiceJob *job_p, const bool required_flag)
{
	bool success_flag = false;
	const json_t *json_value_p = json_object_get (data_p, key_s);

	if (json_value_p)
		{
			if (json_is_integer (json_value_p))
				{
					*value_p = json_integer_value (json_value_p);
					success_flag = true;
				}
			else if (json_is_string (json_value_p))
				{
					const char *value_s = json_string_value (json_value_p);

					if (GetValidInteger (&value_s, value_p))
						{
							success_flag = true;
						}
					else
						{

							AddErrorMessageToServiceJob (job_p, key_s, value_s);
						}

				}		/* else if (json_is_string (json_value_p)) */
			else
				{

				}

		}


	return success_flag;
}



static bool AddPlotsFromJSON (ServiceJob *job_p, const json_t *plots_json_p, Study *study_p, const DFWFieldTrialServiceData *data_p)
{
	OperationStatus status = OS_FAILED;
	bool success_flag	= true;

	if (json_is_array (plots_json_p))
		{
			const size_t num_rows = json_array_size (plots_json_p);
			size_t i;
			size_t num_empty_rows = 0;
			size_t num_imported = 0;
			bool imported_row_flag;

			for (i = 0; i < num_rows; ++ i)
				{
					const json_t *table_row_json_p = json_array_get (plots_json_p, i);

					/*
					 * Is the row non-empty?
					 */
					if (json_object_size (table_row_json_p) > 0)
						{
							const char *gene_bank_s = GetJSONString (table_row_json_p, S_GENE_BANK_S);
							GeneBank *gene_bank_p = NULL;

							imported_row_flag = false;

							if (!gene_bank_s)
								{
									/* default to using the GRU */
									gene_bank_s = "Germplasm Resources Unit";
								}

							gene_bank_p = GetGeneBankByName (gene_bank_s, data_p);

							if (gene_bank_p)
								{
									const char *accession_s = GetJSONString (table_row_json_p, S_ACCESSION_TITLE_S);

									if (!IsStringEmpty (accession_s))
										{
											Material *material_p = GetOrCreateMaterialByAccession (accession_s, gene_bank_p, data_p);

											if (material_p)
												{
													Plot *plot_p = NULL;
													int32 row = -1;

													if (GetJSONStringAsInteger (table_row_json_p, S_ROW_TITLE_S, &row))
														{
															int32 column = -1;

															if (GetJSONStringAsInteger (table_row_json_p, S_COLUMN_TITLE_S, &column))
																{
																	/*
																	 * does the plot already exist?
																	 */
																	plot_p = GetPlotByRowAndColumn (row, column, study_p, data_p);

																	if (!plot_p)
																		{
																			plot_p = CreatePlotFromTabularJSON (table_row_json_p, row, column, study_p, data_p);

																			if (!plot_p)
																				{

																				}		/* if (!plot_p) */

																		}		/* if (!plot_p) */


																	if (plot_p)
																		{
																			/*
																			 * plot_p now has an id, so we can add the row/rack.
																			 */
																			int32 rack_plotwise_index = -1;

																			if (GetJSONStringAsInteger (table_row_json_p, S_RACK_TITLE_S, &rack_plotwise_index))
																				{
																					int32 rack_studywise_index = -1;

																					if (GetJSONStringAsInteger (table_row_json_p, S_INDEX_TITLE_S, &rack_studywise_index))
																						{
																							int32 replicate = 1;
																							const char *rep_s = GetJSONString (table_row_json_p, S_REPLICATE_TITLE_S);
																							bool control_rep_flag = false;
																							Row *row_p = NULL;

																							if (rep_s)
																								{
																									if (Stricmp (rep_s, RO_REPLICATE_CONTROL_S) == 0)
																										{
																											control_rep_flag = true;
																										}
																									else
																										{
																											success_flag = GetValidInteger (&rep_s, &replicate);
																										}		/* if (value_s) */
																								}

																							if (success_flag)
																								{
																									row_p = AllocateRow (NULL, rack_plotwise_index, rack_studywise_index, replicate, material_p, MF_SHALLOW_COPY, plot_p);

																									if (row_p)
																										{
																											if (control_rep_flag)
																												{
																													SetRowGenotypeControl (row_p, true);
																												}

																											if (AddRowToPlot (plot_p, row_p))
																												{
																													if (SavePlot (plot_p, data_p))
																														{
																															++ num_imported;
																															imported_row_flag = true;
																														}
																													else
																														{
																															PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to save row");
																														}

																												}
																											else
																												{
																													FreeRow (row_p);
																												}

																										}
																									else
																										{
																											PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to allocate row");
																										}

																								}		/* if (success_flag) */

																						}		/* if (GetJSONStringAsInteger (table_row_json_p, S_INDEX_TITLE_S, &study_index)) */
																					else
																						{
																							AddErrorForTableCell (job_p, table_row_json_p, i, S_INDEX_TITLE_S, "Value not set");
																							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_INDEX_TITLE_S);
																						}

																				}		/* if (GetJSONStringAsInteger (table_row_json_p, S_RACK_TITLE_S, &rack)) */
																			else
																				{
																					AddErrorForTableCell (job_p, table_row_json_p, i, S_RACK_TITLE_S, "Value not set");
																					PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_RACK_TITLE_S);
																				}

																			FreePlot (plot_p);
																		}		/* if (plot_p) */

																}		/* if (GetJSONStringAsInteger (row_p, S_COLUMN_TITLE_S, &column)) */
															else
																{
																	AddErrorForTableCell (job_p, table_row_json_p, i, S_COLUMN_TITLE_S, "Value not set");
																	PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_COLUMN_TITLE_S);
																}

														}		/* if (GetJSONStringAsInteger (row_p, S_ROW_TITLE_S, &row)) */
													else
														{
															AddErrorForTableCell (job_p, table_row_json_p, i, S_ROW_TITLE_S, "Value not set");
															PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_ROW_TITLE_S);
														}


												}		/* if (material_p) */
											else
												{
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get Material with internal name \"%s\" for area \"%s\"", accession_s, study_p -> st_name_s);
												}

										}		/* if (accession_s) */
									else
										{
											AddErrorForTableCell (job_p, table_row_json_p, i, S_ACCESSION_TITLE_S, "Value not set");
											PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_ACCESSION_TITLE_S);
										}
								}		/* if (gene_bank_p) */
							else
								{
									PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get gene bank \%s\"", gene_bank_s);
								}

							if (!imported_row_flag)
								{
									PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to import plot data");
								}

						}		/* if (json_object_size (table_row_json_p) > 0) */
					else
						{
							++ num_empty_rows;
						}

				}		/* for (i = 0; i < num_rows; ++ i) */

			if (num_imported + num_empty_rows == num_rows)
				{
					status = OS_SUCCEEDED;
				}
			else if (num_imported > 0)
				{
					status = OS_PARTIALLY_SUCCEEDED;
				}

		}		/* if (json_is_array (plots_json_p)) */


	SetServiceJobStatus (job_p, status);

	return success_flag;
}


static Plot *CreatePlotFromTabularJSON (const json_t *table_row_json_p, const int32 row, const int32 column, Study *study_p, const DFWFieldTrialServiceData *data_p)
{
	double width = 0.0;
	Plot *plot_p = NULL;

	if (GetJSONStringAsDouble (table_row_json_p, S_WIDTH_TITLE_S, &width))
		{
			double length = 0.0;

			if (GetJSONStringAsDouble (table_row_json_p, S_LENGTH_TITLE_S, &length))
				{
					const char *treatment_s = GetJSONString (table_row_json_p, S_TREATMENT_TITLE_S);
					const char *comment_s = GetJSONString (table_row_json_p, S_COMMENT_TITLE_S);
					struct tm *sowing_date_p = NULL;
					struct tm *harvest_date_p = NULL;
					const char *date_s = GetJSONString (table_row_json_p, S_SOWING_TITLE_S);


					if (date_s)
						{
							sowing_date_p = GetTimeFromString (date_s);

							if (!sowing_date_p)
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get sowing date from \"%s\"", date_s);
								}
						}

					date_s = GetJSONString (table_row_json_p, S_HARVEST_TITLE_S);
					if (date_s)
						{
							harvest_date_p = GetTimeFromString (date_s);

							if (!harvest_date_p)
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get harvest date from \"%s\"", date_s);
								}
						}

					plot_p = AllocatePlot (NULL, sowing_date_p, harvest_date_p, width, length, row, column, treatment_s, comment_s, study_p);

					if (plot_p)
						{
							if (SavePlot (plot_p, data_p))
								{
									return plot_p;
								}
							else
								{
									PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to save plot");

								}		/* if (!SavePlot (plot_p, data_p)) */

							FreePlot (plot_p);
						}		/* if (plot_p) */
					else
						{
							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to allocate Plot");
						}

				}		/* if (GetJSONStringAsDouble (table_row_json_p, S_LENGTH_TITLE_S, &length)) */
			else
				{
					PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_LENGTH_TITLE_S);
				}

		}		/* if (GetJSONStringAsDouble (table_row_json_p, S_WIDTH_TITLE_S, &width) */
	else
		{
			PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to get \"%s\"", S_WIDTH_TITLE_S);
		}

	return NULL;
}


Plot *GetPlotById (bson_oid_t *id_p, Study *study_p, const DFWFieldTrialServiceData *data_p)
{
	Plot *plot_p = NULL;

	bson_t *query_p = BCON_NEW (MONGO_ID_S, BCON_OID (id_p));

	if (query_p)
		{
			plot_p = GetUniquePlot (query_p, study_p, data_p);

			bson_destroy (query_p);
		}		/* if (query_p) */

	return plot_p;
}


Plot *GetPlotByRowAndColumn (const uint32 row, const uint32 column, Study *study_p, const DFWFieldTrialServiceData *data_p)
{
	Plot *plot_p = NULL;
	bson_t *query_p = BCON_NEW (PL_ROW_INDEX_S, BCON_INT32 (row), PL_COLUMN_INDEX_S, BCON_INT32 (column), PL_PARENT_STUDY_S, BCON_OID (study_p -> st_id_p));

	if (query_p)
		{
			plot_p = GetUniquePlot (query_p, study_p, data_p);

			bson_destroy (query_p);
		}		/* if (query_p) */

	return plot_p;
}



static Plot *GetUniquePlot (bson_t *query_p, Study *study_p, const DFWFieldTrialServiceData *data_p)
{
	Plot *plot_p = NULL;

	if (SetMongoToolCollection (data_p -> dftsd_mongo_p, data_p -> dftsd_collection_ss [DFTD_PLOT]))
		{
			json_t *results_p = GetAllMongoResultsAsJSON (data_p -> dftsd_mongo_p, query_p, NULL);

			if (results_p)
				{
					if (json_is_array (results_p))
						{
							const size_t num_results = json_array_size (results_p);

							if (num_results == 1)
								{
									size_t i = 0;
									json_t *entry_p = json_array_get (results_p, i);

									plot_p = GetPlotFromJSON (entry_p, study_p, data_p);

									if (!plot_p)
										{

										}

								}		/* if (num_results == 1) */

						}		/* if (json_is_array (results_p)) */

					json_decref (results_p);
				}		/* if (results_p) */

		}		/* if (SetMongoToolCollection (data_p -> dftsd_mongo_p, data_p -> dftsd_collection_ss [DFTD_PLOT])) */


	return plot_p;
}


static json_t *GetStudyPlotsForSubmissionTable (Study *study_p, const DFWFieldTrialServiceData *service_data_p)
{
	json_t *plots_table_p = NULL;

	/*
	 * Has the study got any plots?
	 */
	if (study_p -> st_plots_p -> ll_size == 0)
		{
			if (!GetStudyPlots (study_p, service_data_p))
				{

				}

		}		/* if ((study_p -> st_plots_p == NULL) || (study_p -> st_plots_p -> ll_size == 0)) */


	if (study_p -> st_plots_p -> ll_size > 0)
		{
			bool success_flag = true;

			plots_table_p = json_array ();

			if (plots_table_p)
				{
					PlotNode *plot_node_p = (PlotNode *) (study_p -> st_plots_p -> ll_head_p);

					while (success_flag && plot_node_p)
						{
							if (AddPlotRowsToTable (plot_node_p -> pn_plot_p, plots_table_p, service_data_p))
								{
									plot_node_p = (PlotNode *) (plot_node_p -> pn_node.ln_next_p);
								}
							else
								{
									success_flag = false;
								}

						}		/* while (success_flag && plot_node_p) */

					if (!success_flag)
						{
							json_decref (plots_table_p);
						}

				}		/* if (plots_table_p) */

		}		/* if (study_p -> st_plots_p -> ll_size >= 0) */
	else
		{
			/*
			 * Does the study have default values to load?
			 */
			if (HasStudyGotPlotLayoutDetails (study_p))
				{
					plots_table_p = GeneratePlotsTemplate (study_p);
				}
		}

	return plots_table_p;
}


static bool AddPlotRowsToTable (const Plot *plot_p, json_t *plots_table_p, const DFWFieldTrialServiceData *service_data_p)
{
	bool success_flag = true;
	LinkedList *rows_p = plot_p -> pl_rows_p;

	if (rows_p)
		{
			RowNode *row_node_p = (RowNode *) (rows_p -> ll_head_p);

			while (success_flag && row_node_p)
				{
					json_t *row_json_p = GetPlotTableRow (row_node_p -> rn_row_p, service_data_p);

					if (row_json_p)
						{
							if (json_array_append_new (plots_table_p, row_json_p) == 0)
								{
									row_node_p = (RowNode *) (row_node_p -> rn_node.ln_next_p);
								}		/* if (json_array_append_new (plots_json_p, plot_row_p) == 0) */
							else
								{
									json_decref (row_json_p);
									success_flag = false;
								}

						}		/* if (plot_row_p) */
					else
						{
							success_flag = false;
						}

				}		/* while (success_flag && row_node_p) */


		}		/* if (rows_p) */

	return success_flag;
}


static json_t *GeneratePlotsTemplate (const Study *study_p)
{
	json_t *plots_p = json_array ();

	if (plots_p)
		{
			bool success_flag = true;
			const uint32 num_rows = * (study_p -> st_num_rows_p);
			const uint32 num_cols = * (study_p -> st_num_columns_p);
			uint32 i = 1;

			while ((i <= num_rows) && (success_flag == true))
				{
					uint32 j = 1;

					while ((j <= num_cols) && (success_flag == true))
						{
							json_t *row_json_p = GetPlotRowTemplate (i, j, study_p -> st_default_plot_width_p, study_p -> st_default_plot_length_p);

							if (row_json_p)
								{
									if (!json_array_append_new (plots_p, row_json_p) == 0)
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add plot template for [ " UINT32_FMT ", " UINT32_FMT " ]", i, j);
											json_decref (row_json_p);
											success_flag = false;
										}

								}		/* if (plot_row_p) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to generate plot template for [ " UINT32_FMT ", " UINT32_FMT " ]", i, j);
									success_flag = false;
								}

							++ j;
						}		/* while ((j <= num_cols) && (success_flag == true)) */

					++ i;
				}		/* while ((i <= num_rows) && (success_flag == true)) */

			if (success_flag)
				{
					return plots_p;
				}

			json_decref (plots_p);
		}		/* if (plots_p) */

	return NULL;
}



static json_t *GetPlotRowTemplate (const uint32 row, const uint32 column, const double64 *width_p, const double64 *height_p)
{
	json_t *table_row_p = json_object ();

	if (table_row_p)
		{
			if (SetJSONInteger (table_row_p, S_ROW_TITLE_S, row))
				{
					if (SetJSONInteger (table_row_p, S_COLUMN_TITLE_S, column))
						{
							if ((width_p == NULL) || (SetJSONReal (table_row_p, S_WIDTH_TITLE_S, *width_p)))
								{
									if ((height_p == NULL) || (SetJSONReal (table_row_p, S_LENGTH_TITLE_S, *height_p)))
										{
											return table_row_p;
										}
								}
						}
				}

			json_decref (table_row_p);
		}

	return NULL;
}


static json_t *GetPlotTableRow (const Row *row_p, const DFWFieldTrialServiceData *service_data_p)
{
	json_t *table_row_p = json_object ();

	if (table_row_p)
		{
			/*
				if (AddColumnParameterHint (S_ACCESSION_TITLE_S, PT_STRING, hints_p))
			*/
			Plot *plot_p = row_p -> ro_plot_p;

			if ((plot_p -> pl_row_index == 0) || (SetJSONInteger (table_row_p, S_ROW_TITLE_S, plot_p -> pl_row_index)))
				{
					if ((plot_p -> pl_column_index == 0) || (SetJSONInteger (table_row_p, S_COLUMN_TITLE_S, plot_p -> pl_column_index)))
						{
							if ((CompareDoubles (plot_p -> pl_length, 0.0) <= 0) || (SetJSONReal (table_row_p, S_LENGTH_TITLE_S, plot_p -> pl_length)))
								{
									if ((CompareDoubles (plot_p -> pl_width, 0.0) <= 0) || (SetJSONReal (table_row_p, S_WIDTH_TITLE_S, plot_p -> pl_width)))
										{
											if ((plot_p -> pl_comment_s == NULL) || (SetJSONString (table_row_p, S_COMMENT_TITLE_S, plot_p -> pl_comment_s)))
												{
													if ((plot_p -> pl_treatments_s == NULL) || (SetJSONString (table_row_p, S_TREATMENT_TITLE_S, plot_p -> pl_treatments_s)))
														{
															if ((plot_p -> pl_sowing_date_p == NULL) || (AddValidDateToJSON (plot_p -> pl_sowing_date_p, table_row_p, S_SOWING_TITLE_S)))
																{
																	if ((plot_p -> pl_harvest_date_p == NULL) || (AddValidDateToJSON (plot_p -> pl_harvest_date_p, table_row_p, S_HARVEST_TITLE_S)))
																		{
																			if ((row_p -> ro_rack_index == 0) || (SetJSONInteger (table_row_p, S_RACK_TITLE_S, row_p -> ro_rack_index)))
																				{
																					if ((row_p -> ro_replicate_index == 0) || (SetJSONInteger (table_row_p, S_REPLICATE_TITLE_S, row_p -> ro_replicate_index)))
																						{
																							if ((row_p -> ro_by_study_index == 0) || (SetJSONInteger (table_row_p, S_INDEX_TITLE_S, row_p -> ro_by_study_index)))
																								{
																									bool success_flag = false;

																									if (row_p -> ro_material_p)
																										{
																											if ((row_p -> ro_material_p -> ma_accession_s == NULL) || (SetJSONString (table_row_p, S_ACCESSION_TITLE_S, row_p -> ro_material_p -> ma_accession_s)))
																												{

																												}

																											GeneBank *gene_bank_p = GetGeneBankById (row_p -> ro_material_p -> ma_gene_bank_id_p, VF_CLIENT_MINIMAL, service_data_p);

																											if (gene_bank_p)
																												{
																													if (SetJSONString (table_row_p, S_GENE_BANK_S, gene_bank_p -> gb_name_s))
																														{
																															success_flag = true;
																														}

																													FreeGeneBank (gene_bank_p);
																												}
																										}
																									else
																										{
																											success_flag = true;
																										}

																									if (success_flag)
																										{
																											return table_row_p;
																										}


																								}		/* if ((row_p -> ro_by_study_index == 0) || (SetJSONInteger (table_row_p, S_INDEX_TITLE_S, row_p -> ro_by_study_index))) */

																						}		/* if ((row_p -> ro_replicate_index == 0) || (SetJSONInteger (table_row_p, S_REPLICATE_TITLE_S, row_p -> ro_rephttps://www.theguardian.com/ukhttps://www.theguardian.com/uklicate_index))) */

																				}		/* if ((row_p -> ro_rack_index == 0) || (SetJSONInteger (table_row_p, S_RACK_TITLE_S, row_p -> ro_rack_index))) */


																		}		/* if ((plot_p -> pl_harvest_date_p == NULL) || (AddValidDateToJSON (plot_p -> pl_harvest_date_p, table_row_p, S_HARVEST_TITLE_S))) */

																}		/* if ((plot_p -> pl_sowing_date_p == NULL) || (AddValidDateToJSON (plot_p -> pl_sowing_date_p, table_row_p, S_SOWING_TITLE_S))) */

														}		/* if ((plot_p -> pl_treatments_s == NULL) || (SetJSONString (table_row_p, S_TREATMENT_TITLE_S, plot_p -> pl_treatments_s))) */

												}		/* if ((plot_p -> pl_comment_s == NULL) || (SetJSONString (table_row_p, S_COMMENT_TITLE_S, plot_p -> pl_comment_s))) */

										}		/* if ((CompareDoubles (plot_p -> pl_width, 0.0) <= 0) || (SetJSONReal (table_row_p, S_WIDTH_TITLE_S, plot_p -> pl_width))) */

								}		/* if ((CompareDoubles (plot_p -> pl_length, 0.0) <= 0) || (SetJSONReal (table_row_p, S_LENGTH_TITLE_S, plot_p -> pl_length))) */


						}		/* if ((plot_p -> pl_column_index == 0) || (SetJSONInteger (table_row_p, S_COLUMN_TITLE_S, plot_p -> pl_column_index))) */

				}		/* if ((plot_p -> pl_row_index == 0) || (SetJSONInteger (table_row_p, S_ROW_TITLE_S, plot_p -> pl_row_index))) */

			json_decref (table_row_p);
		}		/* if (table_row_p) */

	return NULL;
}


static bool AddErrorForTableCell (ServiceJob *job_p, const json_t *table_row_p, const uint32 row, const char *column_key_s, const char *message_s)
{
	bool success_flag = false;
	json_t *error_p = json_object ();

	if (error_p)
		{
			if (SetJSONInteger (error_p, "row", row))
				{
					if (SetJSONString (error_p, "column", column_key_s))
						{
							if (SetJSONString (error_p, JOB_ERROR_S, message_s))
								{
									if (AddCompoundErrorToServiceJob (job_p, S_PLOT_TABLE.npt_name_s, error_p))
										{
											success_flag = true;
										}
								}
						}
				}

			json_decref (error_p);
		}		/* if (error_p) */

	return success_flag;
}


