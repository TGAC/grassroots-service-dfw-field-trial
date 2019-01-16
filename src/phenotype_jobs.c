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
 * phenotype_jobs.c
 *
 *  Created on: 26 Oct 2018
 *      Author: billy
 */


#include "phenotype_jobs.h"
#include "string_utils.h"
#include "crop_ontology_tool.h"

/*
 * static declarations
 */

static const char S_DEFAULT_COLUMN_DELIMITER =  '|';

static const char * const S_INTERNAL_NAME_TITLE_S = "Accession";
static const char * const S_TRAIT_ID_S = "Trait Identifier";
static const char * const S_TRAIT_ABBREVIATION_S = "Trait Abbreviation";
static const char * const S_TRAIT_NAME_S = "Trait Name";
static const char * const S_TRAIT_DESCRIPTION_S = "Trait Description";
static const char * const S_METHOD_ID_S = "Method Identifier";
static const char * const S_METHOD_ABBREVIATION_S = "Method Abbreviation";
static const char * const S_METHOD_NAME_S = "Method Name";
static const char * const S_METHOD_DESCRIPTION_S = "Method Description";
static const char * const S_UNIT_ID_S = "Unit Identifier";
static const char * const S_UNIT_ABBREVIATION_S = "Unit Abbreviation";
static const char * const S_UNIT_NAME_S = "Unit Name";
static const char * const S_UNIT_DESCRIPTION_S = "Unit Description";


static NamedParameterType S_PHENOTYPE_TABLE_COLUMN_DELIMITER = { "PH Data delimiter", PT_CHAR };
static NamedParameterType S_PHENOTYPE_TABLE = { "PH Upload", PT_TABLE};


static Parameter *GetPhenotypesDataTableParameter (ParameterSet *param_set_p, ParameterGroup *group_p, const DFWFieldTrialServiceData *data_p);


static bool AddPhenotypesFromJSON (ServiceJob *job_p, const json_t *phenotypes_json_p, const DFWFieldTrialServiceData *data_p);

static SchemaTerm *GetSchemaTerm (const json_t *json_p, const char *id_key_s, const char *name_key_s, const char *description_key_s, const char *abbreviation_key_s);




bool AddSubmissionPhenotypeParams (ServiceData *data_p, ParameterSet *param_set_p)
{
	bool success_flag = false;

	ParameterGroup *group_p = CreateAndAddParameterGroupToParameterSet ("Phenotypes", false, data_p, param_set_p);

	if (group_p)
		{
			Parameter *param_p = NULL;
			SharedType def;

			InitSharedType (&def);
			def.st_char_value = S_DEFAULT_COLUMN_DELIMITER;

			if ((param_p = CreateAndAddParameterToParameterSet (data_p, param_set_p, group_p, S_PHENOTYPE_TABLE_COLUMN_DELIMITER.npt_type, false, S_PHENOTYPE_TABLE_COLUMN_DELIMITER.npt_name_s, "Delimiter", "The character delimiting columns", NULL, def, NULL, NULL, PL_ADVANCED, NULL)) != NULL)
				{
					const DFWFieldTrialServiceData *dfw_service_data_p = (DFWFieldTrialServiceData *) data_p;

					def.st_string_value_s = NULL;

					if ((param_p = GetPhenotypesDataTableParameter (param_set_p, group_p, dfw_service_data_p)) != NULL)
						{
							success_flag = true;
						}
				}

		}		/* if (group_p) */


	return success_flag;
}


bool RunForSubmissionPhenotypeParams (DFWFieldTrialServiceData *data_p, ParameterSet *param_set_p, ServiceJob *job_p)
{
	bool job_done_flag = false;

	SharedType value;
	InitSharedType (&value);

	if (GetParameterValueFromParameterSet (param_set_p, S_PHENOTYPE_TABLE.npt_name_s, &value, true))
		{
			/*
			 * Has a spreadsheet been uploaded?
			 */
			if (! (IsStringEmpty (value.st_string_value_s)))
				{
					bool success_flag = false;
					json_error_t e;
					json_t *phenotypes_json_p = NULL;

					job_done_flag = true;

					/*
					 * The data could be either an array of json objects
					 * or a tabular string. so try it as json array first
					 */
					phenotypes_json_p = json_loads (value.st_string_value_s, 0, &e);

					if (phenotypes_json_p)
						{
							if (AddPhenotypesFromJSON (job_p, phenotypes_json_p, data_p))
								{
									success_flag = true;
								}
							else
								{
									PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, phenotypes_json_p, "AddPhenotypesFromJSON for failed");
								}

							json_decref (phenotypes_json_p);
						}		/* if (phenotpnes_json_p) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to load \"%s\" as JSON", value.st_string_value_s);
						}

					job_done_flag = true;
				}		/* if (! (IsStringEmpty (value.st_string_value_s))) */

		}		/* if (GetParameterValueFromParameterSet (param_set_p, S_PHENOTYPE_TABLE.npt_name_s, &value, true)) */


	return job_done_flag;
}


bool GetSubmissionPhenotypeParameterTypeForNamedParameter (const char *param_name_s, ParameterType *pt_p)
{
	bool success_flag = true;

	if (strcmp (param_name_s, S_PHENOTYPE_TABLE_COLUMN_DELIMITER.npt_name_s) == 0)
		{
			*pt_p = S_PHENOTYPE_TABLE_COLUMN_DELIMITER.npt_type;
		}
	else if (strcmp (param_name_s, S_PHENOTYPE_TABLE.npt_name_s) == 0)
		{
			*pt_p = S_PHENOTYPE_TABLE.npt_type;
		}
	else
		{
			success_flag = false;
		}

	return success_flag;
}


Phenotype *GetPhenotypeByInternalName (const char *name_s, const DFWFieldTrialServiceData *data_p)
{
	Phenotype *phenotype_p = NULL;

	if (SetMongoToolCollection (data_p -> dftsd_mongo_p, data_p -> dftsd_collection_ss [DFTD_PHENOTYPE]))
		{
			bson_t *query_p = BCON_NEW (PH_INTERNAL_NAME_S, BCON_UTF8 (name_s));

			if (query_p)
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

											phenotype_p = GetPhenotypeFromJSON (entry_p, data_p);

											if (!phenotype_p)
												{

												}

										}		/* if (num_results == 1) */

								}		/* if (json_is_array (results_p)) */

							json_decref (results_p);
						}		/* if (results_p) */

					bson_destroy (query_p);
				}		/* if (query_p) */

		}		/* if (SetMongoToolCollection (data_p -> dftsd_mongo_p, data_p -> dftsd_collection_ss [DFTD_RAW_PHENOTYPE])) */


	return phenotype_p;

}



/*
 * static definitions
 */


static Parameter *GetPhenotypesDataTableParameter (ParameterSet *param_set_p, ParameterGroup *group_p, const DFWFieldTrialServiceData *data_p)
{
	Parameter *param_p = NULL;
	const char delim_s [2] = { S_DEFAULT_COLUMN_DELIMITER, '\0' };
	char *headers_s = NULL;

	headers_s = ConcatenateVarargsStrings (S_INTERNAL_NAME_TITLE_S, delim_s, S_TRAIT_ID_S, delim_s, S_TRAIT_ABBREVIATION_S, delim_s, S_TRAIT_NAME_S, delim_s, S_TRAIT_DESCRIPTION_S, delim_s,
																				 S_METHOD_ID_S, delim_s, S_METHOD_ABBREVIATION_S, delim_s, S_METHOD_NAME_S, delim_s, S_METHOD_DESCRIPTION_S, delim_s,
																				 S_UNIT_ID_S, delim_s, S_UNIT_ABBREVIATION_S, delim_s, S_UNIT_NAME_S, delim_s, S_UNIT_DESCRIPTION_S, delim_s,
																				 NULL);

	if (headers_s)
		{
			SharedType def;

			InitSharedType (&def);
			def.st_string_value_s = NULL;

			param_p = CreateAndAddParameterToParameterSet (& (data_p -> dftsd_base_data), param_set_p, group_p, S_PHENOTYPE_TABLE.npt_type, false, S_PHENOTYPE_TABLE.npt_name_s, "Phenotype data to upload", "The data to upload", NULL, def, NULL, NULL, PL_ALL, NULL);

			if (param_p)
				{
					bool success_flag = false;

					if (AddParameterKeyValuePair (param_p, PA_TABLE_COLUMN_HEADINGS_S, headers_s))
						{
							if (AddParameterKeyValuePair (param_p, PA_TABLE_COLUMN_DELIMITER_S, delim_s))
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

			FreeCopiedString (headers_s);
		}		/* if (headers_s) */

	return param_p;
}


/*
{
	"Accession": "EM",
	"Trait Identifier": "CO_321:0000007",
	"Trait Abbreviation": "Hd",
	"Trait Name": "Heading time",
	"Trait Description": "Heading time extends from the time of emergence of the tip of the spike from the flag leaf sheath to when the spike has completely emerged but has not yet started to flower.",
	"Method Identifier": "CO_321:0000840",
	"Method Abbreviation": "",
	"Method Name": "Hd DS55 date Estimation",
	"Method Description": "Record date of heading (DS55) when 50% of the spike is emerged (i.e., middle of the spike at the flag leaf ligule) on 50% of all stems.",
	"Unit Identifier": "CO_321:0000855",
	"Unit Abbreviation": "",
	"Unit Name": "Julian date (JD)"
}
 */
static bool AddPhenotypesFromJSON (ServiceJob *job_p, const json_t *phenotypes_json_p, const DFWFieldTrialServiceData *data_p)
{
	bool success_flag	= true;
	OperationStatus status = OS_FAILED;

	if (json_is_array (phenotypes_json_p))
		{
			const size_t num_rows = json_array_size (phenotypes_json_p);
			size_t i;
			size_t num_imported = 0;
			size_t num_empty_rows = 0;

			for (i = 0; i < num_rows; ++ i)
				{
					json_t *table_row_json_p = json_array_get (phenotypes_json_p, i);

					const size_t row_size =  json_object_size (table_row_json_p);

					if (row_size > 0)
						{
							const char *internal_name_s = GetJSONString (table_row_json_p, S_INTERNAL_NAME_TITLE_S);

							if (!IsStringEmpty (internal_name_s))
								{
									Phenotype *phenotype_p = NULL;
									SchemaTerm *trait_p = GetSchemaTerm (table_row_json_p, S_TRAIT_ID_S, S_TRAIT_NAME_S, S_TRAIT_DESCRIPTION_S, S_TRAIT_ABBREVIATION_S);

									if (trait_p)
										{
											SchemaTerm *method_p = GetSchemaTerm (table_row_json_p, S_METHOD_ID_S, S_METHOD_NAME_S, S_METHOD_DESCRIPTION_S, S_METHOD_ABBREVIATION_S);

											if (method_p)
												{
													SchemaTerm *unit_p = GetSchemaTerm (table_row_json_p, S_UNIT_ID_S, S_UNIT_NAME_S, S_UNIT_DESCRIPTION_S, S_UNIT_ABBREVIATION_S);

													if (unit_p)
														{
															phenotype_p = AllocatePhenotype (NULL, trait_p, method_p, unit_p, internal_name_s);

															if (!phenotype_p)
																{
																	FreeSchemaTerm (unit_p);
																}

														}		/* if (unit_p) */

													if (!phenotype_p)
														{
															FreeSchemaTerm (method_p);
														}

												}		/* if (method_p) */

											if (!phenotype_p)
												{
													FreeSchemaTerm (trait_p);
												}

										}		/* if (trait_p) */

									if (phenotype_p)
										{
											if (SavePhenotype (phenotype_p, data_p))
												{
													++ num_imported;
												}
											else
												{
													PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to save Phenotype");
													success_flag = false;
												}

											FreePhenotype (phenotype_p);
										}		/* if (material_p) */
									else
										{
											PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, table_row_json_p, "Failed to allocate Phenotype");
										}
								}

						}		/* if (row_size > 0) */
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


static SchemaTerm *GetSchemaTerm (const json_t *json_p, const char *id_key_s, const char *name_key_s, const char *description_key_s, const char *abbreviation_key_s)
{
	SchemaTerm *term_p = NULL;
	const char *id_s = GetJSONString (json_p, id_key_s);

	if (id_s)
		{
			const char *name_s =  GetJSONString (json_p, name_key_s);
			const char *description_s =  GetJSONString (json_p, description_key_s);
			const char *abbreviation_s =  GetJSONString (json_p, abbreviation_key_s);

			if (!name_s || !description_s || !abbreviation_s)
				{
					if (strncmp (id_s, "CO_", strlen (CONTEXT_PREFIX_CROP_ONTOLOGY_S)) == 0)
						{
							term_p = GetCropOnotologySchemaTerm (id_s);
						}
				}

			if (!term_p)
				{
					if (name_s && description_s)
						{
							term_p = AllocateExtendedSchemaTerm (id_s, name_s, description_s, abbreviation_s);
						}
				}

		}		/* if (id_s) */
	else
		{
			PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, json_p, "Failed to get \"%s\"", id_key_s);
		}

	return term_p;
}


