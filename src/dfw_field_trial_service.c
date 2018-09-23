/*
 ** Copyright 2014-2016 The Earlham Institute
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
#include <string.h>

#include "jansson.h"

#define ALLOCATE_DFW_FIELD_TRIAL_SERVICE_TAGS (1)
#include "dfw_field_trial_service.h"
#include "dfw_field_trial_service_data.h"
#include "memory_allocations.h"
#include "parameter.h"
#include "service_job.h"
#include "mongodb_tool.h"
#include "string_utils.h"
#include "json_tools.h"
#include "grassroots_config.h"
#include "string_linked_list.h"
#include "math_utils.h"
#include "search_options.h"
#include "time_util.h"
#include "io_utils.h"
#include "audit.h"

#include "field_trial.h"
#include "field_trial_sqlite.h"


#ifdef _DEBUG
#define DFW_FIELD_TRIAL_SERVICE_DEBUG	(STM_LEVEL_FINER)
#else
#define DFW_FIELD_TRIAL_SERVICE_DEBUG	(STM_LEVEL_NONE)
#endif


/*
 * Field Trial parameters
 */
static NamedParameterType DFTS_FIELD_TRIAL_NAME = { "Name", PT_STRING };
static NamedParameterType DFTS_FIELD_TRIAL_TEAM = { "Team", PT_STRING };
static NamedParameterType DFTS_ADD_FIELD_TRIAL = { "Add Field Trial", PT_BOOLEAN };
static NamedParameterType DFTS_GET_ALL_FIELD_TRIALS = { "Get all Field Trials", PT_BOOLEAN };



static const char *s_data_names_pp [DFTD_NUM_TYPES];


static const char S_DEFAULT_COLUMN_DELIMITER =  '|';


/*
 * STATIC PROTOTYPES
 */

static Service *GetDFWFieldTrialService (void);

static const char *GetDFWFieldTrialServiceName (Service *service_p);

static const char *GetDFWFieldTrialServiceDesciption (Service *service_p);

static const char *GetDFWFieldTrialServiceInformationUri (Service *service_p);

static ParameterSet *GetDFWFieldTrialServiceParameters (Service *service_p, Resource *resource_p, UserDetails *user_p);

static void ReleaseDFWFieldTrialServiceParameters (Service *service_p, ParameterSet *params_p);

static ServiceJobSet *RunDFWFieldTrialService (Service *service_p, ParameterSet *param_set_p, UserDetails *user_p, ProvidersStateTable *providers_p);

static  ParameterSet *IsResourceForDFWFieldTrialService (Service *service_p, Resource *resource_p, Handler *handler_p);


static DFWFieldTrialServiceData *AllocateDFWFieldTrialServiceData (void);

static json_t *GetDFWFieldTrialServiceResults (Service *service_p, const uuid_t job_id);

static bool ConfigureDFWFieldTrialService (DFWFieldTrialServiceData *data_p);


static void FreeDFWFieldTrialServiceData (DFWFieldTrialServiceData *data_p);

static bool CloseDFWFieldTrialService (Service *service_p);


static uint32 InsertData (MongoTool *tool_p, ServiceJob *job_p, json_t *values_p, const DFWFieldTrialData collection_type, DFWFieldTrialServiceData *service_data_p);


static OperationStatus SearchData (MongoTool *tool_p, ServiceJob *job_p, json_t *data_p, const DFWFieldTrialData collection_type, DFWFieldTrialServiceData *service_data_p, const bool preview_flag);


static uint32 DeleteData (MongoTool *tool_p, ServiceJob *job_p, json_t *data_p, const DFWFieldTrialData collection_type, DFWFieldTrialServiceData *service_data_p);

static bool AddFieldTrialParams (ServiceData *data_p, ParameterSet *param_set_p);

static int RunForFieldTrialParams (DFWFieldTrialServiceData *data_p, ParameterSet *param_set_p);


static json_t *ConvertToResource (const size_t i, json_t *src_record_p);


static ServiceMetadata *GetDFWFieldTrialServiceMetadata (Service *service_p);

/*
static const char *InsertFieldData (MongoTool *tool_p, json_t *values_p, DFWFieldTrialServiceData *data_p);
static const char *InsertPlotData (MongoTool *tool_p, json_t *values_p, DFWFieldTrialServiceData *data_p);
static const char *InsertDrillingData (MongoTool *tool_p, json_t *values_p, DFWFieldTrialServiceData *data_p);
static const char *InsertRawPhenotypeData (MongoTool *tool_p, json_t *values_p, DFWFieldTrialServiceData *data_p);
static const char *InsertCorrectedPhenotypeData (MongoTool *tool_p, json_t *values_p, DFWFieldTrialServiceData *data_p);
*/

/*
 * API FUNCTIONS
 */


ServicesArray *GetServices (UserDetails *user_p)
{
	ServicesArray *services_p = AllocateServicesArray (1);

	if (services_p)
		{
			Service *service_p = GetDFWFieldTrialService ();

			if (service_p)
				{
					* (services_p -> sa_services_pp) = service_p;
					return services_p;
				}

			FreeServicesArray (services_p);
		}

	return NULL;
}


void ReleaseServices (ServicesArray *services_p)
{
	FreeServicesArray (services_p);
}


static json_t *GetDFWFieldTrialServiceResults (Service *service_p, const uuid_t job_id)
{
	DFWFieldTrialServiceData *data_p = (DFWFieldTrialServiceData *) (service_p -> se_data_p);
	ServiceJob *job_p = GetServiceJobFromServiceJobSetById (service_p -> se_jobs_p, job_id);
	json_t *res_p = NULL;

	if (job_p)
		{
			if (job_p -> sj_status == OS_SUCCEEDED)
				{
					json_error_t error;
					//const char *buffer_data_p = GetCurlToolData (data_p -> wsd_curl_data_p);
					//res_p = json_loads (buffer_data_p, 0, &error);
				}
		}		/* if (job_p) */

	return res_p;
}



static Service *GetDFWFieldTrialService (void)
{
	Service *service_p = (Service *) AllocMemory (sizeof (Service));

	if (service_p)
		{
			DFWFieldTrialServiceData *data_p = AllocateDFWFieldTrialServiceData ();

			if (data_p)
				{
					if (InitialiseService (service_p,
														 GetDFWFieldTrialServiceName,
														 GetDFWFieldTrialServiceDesciption,
														 GetDFWFieldTrialServiceInformationUri,
														 RunDFWFieldTrialService,
														 IsResourceForDFWFieldTrialService,
														 GetDFWFieldTrialServiceParameters,
														 ReleaseDFWFieldTrialServiceParameters,
														 CloseDFWFieldTrialService,
														 NULL,
														 false,
														 SY_SYNCHRONOUS,
														 (ServiceData *) data_p,
														 GetDFWFieldTrialServiceMetadata))
						{

							if (ConfigureDFWFieldTrialService (data_p))
								{
									* (s_data_names_pp + DFTD_FIELD_TRIAL) = DFT_FIELD_S;
									* (s_data_names_pp + DFTD_PLOT) = DFT_PLOT_S;
									* (s_data_names_pp + DFTD_DRILLING) = DFT_DRILLING_S;
									* (s_data_names_pp + DFTD_RAW_PHENOTYPE) = DFT_RAW_PHENOTYPE_S;
									* (s_data_names_pp + DFTD_CORRECTED_PHENOTYPE) = DFT_CORRECTED_PHENOTYPE_S;

									return service_p;
								}

						}		/* if (InitialiseService (.... */

					FreeDFWFieldTrialServiceData (data_p);
				}

			FreeMemory (service_p);
		}		/* if (service_p) */

	return NULL;
}


static bool ConfigureDFWFieldTrialService (DFWFieldTrialServiceData *data_p)
{
	bool success_flag = false;
	const json_t *service_config_p = data_p -> dftsd_base_data.sd_config_p;

	data_p -> dftsd_database_s = GetJSONString (service_config_p, "database");

	if (data_p -> dftsd_database_s)
		{
			const char *value_s = GetJSONString (service_config_p, "database_type");

			if (value_s)
				{
					if (strcmp (value_s, "mongodb") == 0)
						{
							data_p -> dftsd_backend = DB_MONGO_DB;
							data_p -> dftsd_mongo_p = AllocateMongoTool (NULL);

							if (data_p -> dftsd_mongo_p)
								{
									success_flag = true;
								}
						}
					else if (strcmp (value_s, "sqlite") == 0)
						{
							data_p -> dftsd_backend = DB_SQLITE;

							data_p -> dftsd_sqlite_p = AllocateSQLiteTool (data_p -> dftsd_database_s, SQLITE_OPEN_READWRITE);

							if (data_p -> dftsd_sqlite_p)
								{
									success_flag = true;
								}

						}
					else
						{
							data_p -> dftsd_backend = DB_NUM_BACKENDS;
						}

					if (success_flag)
						{
							value_s = GetJSONString (service_config_p, "field_trial_table");

							if (value_s)
								{
									* (data_p -> dftsd_collection_ss + DFTD_FIELD_TRIAL) = value_s;
								}

							if ((value_s = GetJSONString (service_config_p, "plot_table")) != NULL)
								{
									* (data_p -> dftsd_collection_ss + DFTD_PLOT) = value_s;
								}

						}

				}

		} /* if (data_p -> psd_database_s) */

	return success_flag;
}


static DFWFieldTrialServiceData *AllocateDFWFieldTrialServiceData (void)
{

	DFWFieldTrialServiceData *data_p = (DFWFieldTrialServiceData *) AllocMemory (sizeof (DFWFieldTrialServiceData));

	if (data_p)
		{
			data_p -> dftsd_mongo_p = NULL;
			data_p -> dftsd_database_s = NULL;
			data_p -> dftsd_sqlite_p = NULL;
			data_p -> dftsd_backend = DB_NUM_BACKENDS;

			data_p -> dftsd_collection_ss [DFTD_FIELD_TRIAL] = "FieldTrial";
			data_p -> dftsd_collection_ss [DFTD_EXPERIMENTAL_AREA] = "ExperimentalArea";
			data_p -> dftsd_collection_ss [DFTD_PLOT] = "Plot";
			data_p -> dftsd_collection_ss [DFTD_ROW] = "Row";
			data_p -> dftsd_collection_ss [DFTD_RAW_PHENOTYPE] = "Phenotype";
		}

	return data_p;
}


static void FreeDFWFieldTrialServiceData (DFWFieldTrialServiceData *data_p)
{
	if (data_p -> dftsd_mongo_p)
		{
			FreeMongoTool (data_p -> dftsd_mongo_p);
		}

	if (data_p -> dftsd_sqlite_p)
		{
			FreeSQLiteTool (data_p -> dftsd_sqlite_p);
		}


	FreeMemory (data_p);
}


static const char *GetDFWFieldTrialServiceName (Service * UNUSED_PARAM (service_p))
{
	return "DFWFieldTrial service";
}


static const char *GetDFWFieldTrialServiceDesciption (Service * UNUSED_PARAM (service_p))
{
	return "A service to analyse the spread of Wheat-related diseases both geographically and temporally";
}


static const char *GetDFWFieldTrialServiceInformationUri (Service * UNUSED_PARAM (service_p))
{
	return NULL;
}


static ParameterSet *GetDFWFieldTrialServiceParameters (Service *service_p, Resource * UNUSED_PARAM (resource_p), UserDetails * UNUSED_PARAM (user_p))
{
	ParameterSet *params_p  = AllocateParameterSet ("DFWFieldTrial service parameters", "The parameters used for the DFWFieldTrial service");

	if (params_p)
		{
			if (AddFieldTrialParams (service_p -> se_data_p, params_p))
				{
					return params_p;
				}

			FreeParameterSet (params_p);
		}

	return NULL;
}


static int RunForFieldTrialParams (DFWFieldTrialServiceData *data_p, ParameterSet *param_set_p)
{
	int res = 0;
	SharedType value;
	InitSharedType (&value);

	if (GetParameterValueFromParameterSet (param_set_p, DFTS_ADD_FIELD_TRIAL.npt_name_s, &value, true))
		{
			if (value.st_boolean_value)
				{
					if (GetParameterValueFromParameterSet (param_set_p, DFTS_FIELD_TRIAL_NAME.npt_name_s, &value, true))
						{
							SharedType team_value;
							InitSharedType (&team_value);

							if (GetParameterValueFromParameterSet (param_set_p, DFTS_FIELD_TRIAL_TEAM.npt_name_s, &team_value, true))
								{
									FieldTrial *trial_p = AllocateFieldTrial (value.st_string_value_s, team_value.st_string_value_s, NULL);

									if (trial_p)
										{
											if (SaveFieldTrial (trial_p, data_p))
												{
													res = 1;
												}
											else
												{
													res = -1;
												}

											FreeFieldTrial (trial_p);
										}
								}

						}

				}		/* if (value.st_boolean_value) */

		}		/* if (GetParameterValueFromParameterSet (param_set_p, DFTS_ADD_FIELD_TRIAL.npt_name_s, &value, true)) */

	return res;
}

static bool AddFieldTrialParams (ServiceData *data_p, ParameterSet *param_set_p)
{
	bool success_flag = false;
	Parameter *param_p = NULL;
	SharedType def;
	ParameterGroup *group_p = CreateAndAddParameterGroupToParameterSet ("Field Trials", NULL, false, data_p, param_set_p);

	def.st_string_value_s = NULL;

	if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, NULL, DFTS_FIELD_TRIAL_NAME.npt_type, DFTS_FIELD_TRIAL_NAME.npt_name_s, "Name", "The name of the Field Trial", def, PL_BASIC)) != NULL)
		{
			if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, NULL, DFTS_FIELD_TRIAL_TEAM.npt_type, DFTS_FIELD_TRIAL_TEAM.npt_name_s, "Team", "The team name of the Field Trial", def, PL_BASIC)) != NULL)
				{
					def.st_boolean_value = false;

					if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, NULL, DFTS_ADD_FIELD_TRIAL.npt_type, DFTS_ADD_FIELD_TRIAL.npt_name_s, "Add", "Add a new Field Trial", def, PL_BASIC)) != NULL)
						{
							if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, param_set_p, NULL, DFTS_GET_ALL_FIELD_TRIALS.npt_type, DFTS_GET_ALL_FIELD_TRIALS.npt_name_s, "List", "Get all of the existing Field Trials", def, PL_BASIC)) != NULL)
								{
									success_flag = true;
								}
						}
				}
		}

	return success_flag;

}




static void ReleaseDFWFieldTrialServiceParameters (Service * UNUSED_PARAM (service_p), ParameterSet *params_p)
{
	FreeParameterSet (params_p);
}




static bool CloseDFWFieldTrialService (Service *service_p)
{
	bool success_flag = true;

	FreeDFWFieldTrialServiceData ((DFWFieldTrialServiceData *) (service_p -> se_data_p));;

	return success_flag;
}



static ServiceJobSet *RunDFWFieldTrialService (Service *service_p, ParameterSet *param_set_p, UserDetails * UNUSED_PARAM (user_p), ProvidersStateTable * UNUSED_PARAM (providers_p))
{
	DFWFieldTrialServiceData *data_p = (DFWFieldTrialServiceData *) (service_p -> se_data_p);

	service_p -> se_jobs_p = AllocateSimpleServiceJobSet (service_p, NULL, "DFWFieldTrial");

	if (service_p -> se_jobs_p)
		{
			ServiceJob *job_p = GetServiceJobFromServiceJobSet (service_p -> se_jobs_p, 0);

			LogParameterSet (param_set_p, job_p);

			SetServiceJobStatus (job_p, OS_FAILED_TO_START);

			if (param_set_p)
				{
					int res = RunForFieldTrialParams (data_p, param_set_p);

				}		/* if (param_set_p) */

#if DFW_FIELD_TRIAL_SERVICE_DEBUG >= STM_LEVEL_FINE
			PrintJSONToLog (STM_LEVEL_FINE, __FILE__, __LINE__, job_p -> sj_metadata_p, "metadata 3: ");
#endif

			LogServiceJob (job_p);
		}		/* if (service_p -> se_jobs_p) */

	return service_p -> se_jobs_p;
}




static json_t *ConvertToResource (const size_t i, json_t *src_record_p)
{
	json_t *resource_p = NULL;
	char *title_s = ConvertUnsignedIntegerToString (i);

	if (title_s)
		{
			resource_p = GetResourceAsJSONByParts (PROTOCOL_INLINE_S, NULL, title_s, src_record_p);

			FreeCopiedString (title_s);
		}		/* if (raw_result_p) */

	return resource_p;
}


static OperationStatus SearchData (MongoTool *tool_p, ServiceJob *job_p, json_t *data_p, const DFWFieldTrialData collection_type, DFWFieldTrialServiceData * UNUSED_PARAM (service_data_p), const bool preview_flag)
{
	OperationStatus status = OS_FAILED;
	json_t *values_p = json_object_get (data_p, MONGO_OPERATION_DATA_S);

	if (values_p)
		{
			const char **fields_ss = NULL;
			json_t *fields_p = json_object_get (data_p, MONGO_OPERATION_FIELDS_S);

			if (fields_p)
				{
					if (json_is_array (fields_p))
						{
							size_t size = json_array_size (fields_p);

							fields_ss = (const char **) AllocMemoryArray (size + 1, sizeof (const char *));

							if (fields_ss)
								{
									const char **field_ss = fields_ss;
									size_t i;
									json_t *field_p;

									json_array_foreach (fields_p, i, field_p)
										{
											if (json_is_string (field_p))
												{
													*field_ss = json_string_value (field_p);
													++ field_ss;
												}
											else
												{
													char *dump_s = json_dumps (field_p, JSON_INDENT (2));

													PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to get field from %s", dump_s);
													free (dump_s);
												}
										}

								}		/* if (fields_ss) */

						}		/* if (json_is_array (fields_p)) */

				}		/* if (fields_p) */

			if (FindMatchingMongoDocumentsByJSON (tool_p, values_p, fields_ss))
				{
					json_t *raw_results_p = GetAllExistingMongoResultsAsJSON (tool_p);

					if (raw_results_p)
						{
							if (json_is_array (raw_results_p))
								{
									const size_t size = json_array_size (raw_results_p);
									size_t i = 0;
									json_t *raw_result_p = NULL;
									char *date_s = NULL;
									bool success_flag = true;

									/*
									 * If we are on the public view, we need to filter
									 * out entries that don't meet the live dates.
									 */
									if (!preview_flag)
										{
											struct tm current_time;

											if (GetCurrentTime (&current_time))
												{
													date_s = GetTimeAsString (&current_time);

													if (!date_s)
														{
															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to convert time to a string");
															success_flag = false;
														}
												}
											else
												{
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get current time");
													success_flag = false;
												}

										}		/* if (!private_view_flag) */

									if (success_flag)
										{
											for (i = 0; i < size; ++ i)
												{
													raw_result_p = json_array_get (raw_results_p, i);
													char *title_s = ConvertUnsignedIntegerToString (i + 1);

													if (!title_s)
														{
															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to convert " SIZET_FMT " to string for title", i + 1);
														}

													/* We don't need to return the internal mongo id so remove it */
													json_object_del (raw_result_p, MONGO_ID_S);

													if (raw_result_p)
														{
															json_t *resource_p = GetResourceAsJSONByParts (PROTOCOL_INLINE_S, NULL, title_s, raw_result_p);

															if (resource_p)
																{
																	if (!AddResultToServiceJob (job_p, resource_p))
																		{
																			AddErrorToServiceJob (job_p, title_s, "Failed to add result");

																			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add json resource for " SIZET_FMT " to results array", i);
																			json_decref (resource_p);
																		}
																}		/* if (resource_p) */
															else
																{
																	PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create json resource for " SIZET_FMT, i);
																}

														}		/* if (raw_result_p) */

													if (title_s)
														{
															FreeCopiedString (title_s);
														}
												}		/* for (i = 0; i < size; ++ i) */


											i = GetNumberOfServiceJobResults (job_p);
											status = (i == json_array_size (raw_results_p)) ? OS_SUCCEEDED : OS_PARTIALLY_SUCCEEDED;
										}		/* if (success_flag) */

									if (date_s)
										{
											FreeCopiedString (date_s);
										}

								}		/* if (json_is_array (raw_results_p)) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Search results is not an array");
								}

							json_decref (raw_results_p);
						}		/* if (raw_results_p) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Couldn't get raw results from search");
						}
				}		/* if (FindMatchingMongoDocumentsByJSON (tool_p, values_p, fields_ss)) */
			else
				{
#if DFW_FIELD_TRIAL_SERVICE_DEBUG >= STM_LEVEL_FINE
					PrintJSONToLog (STM_LEVEL_SEVERE, __FILE__, __LINE__, values_p, "No results found for ");
#endif
				}

			if (fields_ss)
				{
					FreeMemory (fields_ss);
				}
		}		/* if (values_p) */


#if DFW_FIELD_TRIAL_SERVICE_DEBUG >= STM_LEVEL_FINE
	PrintJSONToLog (STM_LEVEL_SEVERE, __FILE__, __LINE__, job_p -> sj_result_p, "results_p: ");
#endif

	SetServiceJobStatus (job_p, status);

	return status;
}


static char *CheckDataIsValid (const json_t *row_p, DFWFieldTrialServiceData *data_p)
{
	char *errors_s = NULL;

	return errors_s;
}


/*
bool AddErrorMessage (json_t *errors_p, const json_t *values_p, const size_t row, const char * const error_s)
{
	bool success_flag = false;
	const char *pathogenomics_id_s = GetJSONString (values_p, PG_ID_S);

	if (pathogenomics_id_s)
		{
			json_error_t error;
			json_t *error_p = json_pack_ex (&error, 0, "{s:s,s:i,s:s}", "ID", pathogenomics_id_s, "row", row, "error", error_s);

			if (error_p)
				{
					if (json_array_append_new (errors_p, error_p) == 0)
						{
							success_flag = true;
						}
				}
		}

#if DFW_FIELD_TRIAL_SERVICE_DEBUG >= STM_LEVEL_FINE
	PrintJSONToLog (STM_LEVEL_FINE, __FILE__, __LINE__, errors_p, "errors data: ");
#endif

	return success_flag;
}
*/


static uint32 InsertData (MongoTool *tool_p, ServiceJob *job_p, json_t *values_p, const DFWFieldTrialData collection_type, DFWFieldTrialServiceData *data_p)
{
	uint32 num_imports = 0;
	const char *(*insert_fn) (MongoTool *tool_p, json_t *values_p, DFWFieldTrialServiceData *data_p) = NULL;

	#if DFW_FIELD_TRIAL_SERVICE_DEBUG >= STM_LEVEL_FINE
	PrintJSONToLog (STM_LEVEL_FINE, __FILE__, __LINE__, values_p, "values_p: ");
	#endif

	/*
	switch (collection_type)
		{
			case DFTD_FIELD:
				insert_fn = InsertFieldData;
				break;

			case DFTD_PLOT:
				insert_fn = InsertPlotData;
				break;

			case DFTD_DRILLING:
				insert_fn = InsertDrillingData;
				break;

			case DFTD_RAW_PHENOTYPE:
				insert_fn = InsertRawPhenotypeData;
				break;

			case DFTD_CORRECTED_PHENOTYPE:
				insert_fn = InsertCorrectedPhenotypeData;
				break;

			default:
				break;
		}
*/

	if (insert_fn)
		{
			if (json_is_array (values_p))
				{
					json_t *value_p;
					size_t i;

					json_array_foreach (values_p, i, value_p)
						{
							const char *error_s = insert_fn (tool_p, value_p, data_p);

							if (error_s)
								{
									AddErrorMessage (job_p, value_p, error_s, i);
								}
							else
								{
									++ num_imports;
								}
						}
				}
			else
				{
					const char *error_s = insert_fn (tool_p, values_p, data_p);

					if (error_s)
						{
							AddErrorMessage (job_p, values_p, error_s, 0);

							PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "%s", error_s);
						}
					else
						{
							++ num_imports;
						}
				}
		}


	return num_imports;
}


bool AddErrorMessage (ServiceJob *job_p, const json_t *value_p, const char *error_s, const int index)
{
	char *dump_s = json_dumps (value_p, JSON_INDENT (2) | JSON_PRESERVE_ORDER);
	const char *id_s = GetJSONString (value_p, "id");
	bool added_error_flag = false;


	if (id_s)
		{
			added_error_flag = AddErrorToServiceJob (job_p, id_s, error_s);
		}
	else
		{
			char *index_s = GetIntAsString (index);

			if (index_s)
				{
					char *row_s = ConcatenateStrings ("row ", index_s);

					if (row_s)
						{
							added_error_flag = AddErrorToServiceJob (job_p, row_s, error_s);

							FreeCopiedString (row_s);
						}

					FreeCopiedString (index_s);
				}

		}

	if (!added_error_flag)
		{
			PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "failed to add %s to client feedback messsage", error_s);
		}


	if (dump_s)
		{
			PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to import \"%s\": error=%s", dump_s, error_s);
			free (dump_s);
		}
	else
		{
			PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to import error=%s", dump_s, error_s);
		}

	return added_error_flag;
}


static uint32 DeleteData (MongoTool *tool_p, ServiceJob * UNUSED_PARAM (job_p), json_t *data_p, const DFWFieldTrialData UNUSED_PARAM (collection_type), DFWFieldTrialServiceData * UNUSED_PARAM (service_data_p))
{
	bool success_flag = false;
	json_t *selector_p = json_object_get (data_p, MONGO_OPERATION_DATA_S);

	if (selector_p)
		{
			success_flag = RemoveMongoDocuments (tool_p, selector_p, false);
		}		/* if (values_p) */

	return success_flag ? 1 : 0;
}


static ParameterSet *IsResourceForDFWFieldTrialService (Service * UNUSED_PARAM (service_p), Resource * UNUSED_PARAM (resource_p), Handler * UNUSED_PARAM (handler_p))
{
	return NULL;
}


static ServiceMetadata *GetDFWFieldTrialServiceMetadata (Service *service_p)
{
	const char *term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "topic_0625";
	SchemaTerm *category_p = AllocateSchemaTerm (term_url_s, "Genotype and phenotype",
		"The study of genetic constitution of a living entity, such as an individual, and organism, a cell and so on, "
		"typically with respect to a particular observable phenotypic traits, or resources concerning such traits, which "
		"might be an aspect of biochemistry, physiology, morphology, anatomy, development and so on.");

	if (category_p)
		{
			SchemaTerm *subcategory_p;

			term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "operation_0304";
			subcategory_p = AllocateSchemaTerm (term_url_s, "Query and retrieval", "Search or query a data resource and retrieve entries and / or annotation.");

			if (subcategory_p)
				{
					ServiceMetadata *metadata_p = AllocateServiceMetadata (category_p, subcategory_p);

					if (metadata_p)
						{
							SchemaTerm *input_p;

							term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "data_0968";
							input_p = AllocateSchemaTerm (term_url_s, "Keyword",
								"Boolean operators (AND, OR and NOT) and wildcard characters may be allowed. Keyword(s) or phrase(s) used (typically) for text-searching purposes.");

							if (input_p)
								{
									if (AddSchemaTermToServiceMetadataInput (metadata_p, input_p))
										{
											SchemaTerm *output_p;

											/* Place */
											term_url_s = CONTEXT_PREFIX_SCHEMA_ORG_S "Place";
											output_p = AllocateSchemaTerm (term_url_s, "Place", "Entities that have a somewhat fixed, physical extension.");

											if (output_p)
												{
													if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
														{
															/* Date */
															term_url_s = CONTEXT_PREFIX_SCHEMA_ORG_S "Date";
															output_p = AllocateSchemaTerm (term_url_s, "Date", "A date value in ISO 8601 date format.");

															if (output_p)
																{
																	if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																		{
																			/* Pathogen */
																			term_url_s = CONTEXT_PREFIX_EXPERIMENTAL_FACTOR_ONTOLOGY_S "EFO_0000643";
																			output_p = AllocateSchemaTerm (term_url_s, "pathogen", "A biological agent that causes disease or illness to its host.");

																			if (output_p)
																				{
																					if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																						{
																							/* Phenotype */
																							term_url_s = CONTEXT_PREFIX_EXPERIMENTAL_FACTOR_ONTOLOGY_S "EFO_0000651";
																							output_p = AllocateSchemaTerm (term_url_s, "phenotype", "The observable form taken by some character (or group of characters) "
																								"in an individual or an organism, excluding pathology and disease. The detectable outward manifestations of a specific genotype.");

																							if (output_p)
																								{
																									if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																										{
																											/* Genotype */
																											term_url_s = CONTEXT_PREFIX_EXPERIMENTAL_FACTOR_ONTOLOGY_S "EFO_0000513";
																											output_p = AllocateSchemaTerm (term_url_s, "genotype", "Information, making the distinction between the actual physical material "
																												"(e.g. a cell) and the information about the genetic content (genotype).");

																											if (output_p)
																												{
																													if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																														{
																															return metadata_p;
																														}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																													else
																														{
																															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																															FreeSchemaTerm (output_p);
																														}

																												}		/* if (output_p) */
																											else
																												{
																													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																												}
																										}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																									else
																										{
																											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																											FreeSchemaTerm (output_p);
																										}

																								}		/* if (output_p) */
																							else
																								{
																									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																								}

																						}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																					else
																						{
																							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																							FreeSchemaTerm (output_p);
																						}

																				}		/* if (output_p) */
																			else
																				{
																					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																				}

																		}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																	else
																		{
																			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																			FreeSchemaTerm (output_p);
																		}

																}		/* if (output_p) */
															else
																{
																	PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																}


														}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
													else
														{
															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
															FreeSchemaTerm (output_p);
														}

												}		/* if (output_p) */
											else
												{
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
												}

										}		/* if (AddSchemaTermToServiceMetadataInput (metadata_p, input_p)) */
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add input term %s to service metadata", term_url_s);
											FreeSchemaTerm (input_p);
										}

								}		/* if (input_p) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate input term %s for service metadata", term_url_s);
								}

						}		/* if (metadata_p) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate service metadata");
						}

				}		/* if (subcategory_p) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate sub-category term %s for service metadata", term_url_s);
				}

		}		/* if (category_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate category term %s for service metadata", term_url_s);
		}

	return NULL;
}


