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
 * field_trial_mongodb.h
 *
 *  Created on: 12 Sep 2018
 *      Author: billy
 */

#ifndef SERVICES_FIELD_TRIALS_INCLUDE_FIELD_TRIAL_MONGODB_H_
#define SERVICES_FIELD_TRIALS_INCLUDE_FIELD_TRIAL_MONGODB_H_

#include "dfw_field_trial_service_data.h"
#include "field_trial.h"


#ifdef __cplusplus
extern "C"
{
#endif


DFW_FIELD_TRIAL_SERVICE_LOCAL LinkedList *GetFieldTrialsByNameFromMongoDB (const FieldTrialServiceData *data_p, const char *name_s);


DFW_FIELD_TRIAL_SERVICE_LOCAL LinkedList *GetFieldTrialsByTeamFromMongoDB (const FieldTrialServiceData *data_p, const char *team_s);


DFW_FIELD_TRIAL_SERVICE_LOCAL FieldTrial *GetFieldTrialFromMongoDB (const FieldTrialServiceData *data_p, const char *name_s, const char *team_s);


DFW_FIELD_TRIAL_SERVICE_LOCAL bool AddFieldTrialByNameToMongoDB (FieldTrialServiceData *data_p, FieldTrial *trial_p);


#ifdef __cplusplus
}
#endif


#endif /* SERVICES_FIELD_TRIALS_INCLUDE_FIELD_TRIAL_MONGODB_H_ */
