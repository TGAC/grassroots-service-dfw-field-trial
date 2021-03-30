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
 * plot_jobs.h
 *
 *  Created on: 1 Oct 2018
 *      Author: billy
 */

#ifndef SERVICES_FIELD_TRIALS_INCLUDE_PLOT_JOBS_H_
#define SERVICES_FIELD_TRIALS_INCLUDE_PLOT_JOBS_H_


#include "dfw_field_trial_service_data.h"
#include "dfw_field_trial_service_library.h"

#include "study.h"
#include "plot.h"


#ifdef __cplusplus
extern "C"
{
#endif



DFW_FIELD_TRIAL_SERVICE_LOCAL bool AddSubmissionPlotParams (ServiceData *data_p, ParameterSet *param_set_p, Resource *resource_p);


DFW_FIELD_TRIAL_SERVICE_LOCAL bool RunForSubmissionPlotParams (FieldTrialServiceData *data_p, ParameterSet *param_set_p, ServiceJob *job_p);


DFW_FIELD_TRIAL_SERVICE_LOCAL bool GetSubmissionPlotParameterTypeForNamedParameter (const char *param_name_s, ParameterType *pt_p);


DFW_FIELD_TRIAL_SERVICE_LOCAL Plot *GetPlotByRowAndColumn (const uint32 row, const uint32 column, Study *area_p, const FieldTrialServiceData *data_p);


DFW_FIELD_TRIAL_SERVICE_LOCAL Plot *GetPlotById (bson_oid_t *id_p, Study *study_p, const FieldTrialServiceData *data_p);


/**
 * Get the plots as a Frictionless Data Table
 *
 * @param study_p The Study to get the plots for.
 * @param data_p The Field Trial Service Config
 * @return The JSON for the plots or <code>NULL</code> upon error.
 */
DFW_FIELD_TRIAL_SERVICE_LOCAL json_t *GetPlotsAsFDTabularPackage (const Study *study_p, const FieldTrialServiceData *data_p);


DFW_FIELD_TRIAL_SERVICE_LOCAL json_t *GetStudyPlotHeaderAsFrictionlessData (void);

DFW_FIELD_TRIAL_SERVICE_LOCAL json_t *GetPlotsFrictionlessDataTableSchema (void);


DFW_FIELD_TRIAL_SERVICE_LOCAL json_t *GetPlotsCSVDialect (void);

DFW_FIELD_TRIAL_SERVICE_LOCAL json_t *GetPlotAsFrictionlessData (const Plot *plot_p, const FieldTrialServiceData *service_data_p, const char * const null_sequence_s);


#ifdef __cplusplus
}
#endif



#endif /* SERVICES_FIELD_TRIALS_INCLUDE_PLOT_JOBS_H_ */
