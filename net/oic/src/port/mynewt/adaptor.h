/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef ADAPTOR_H
#define ADAPTOR_H

#ifdef __cplusplus
extern "C" {
#endif

struct os_eventq *oc_evq_get(void);
void oc_evq_set(struct os_eventq *evq);

struct oc_message_s;

#if (MYNEWT_VAL(OC_TRANSPORT_IP) == 1)
int oc_connectivity_init_ip(void);
void oc_connectivity_shutdown_ip(void);
void oc_send_buffer_ip(struct oc_message_s *message);
void oc_send_buffer_ip_mcast(struct oc_message_s *message);
struct oc_message_s *oc_attempt_rx_ip(void);
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_GATT) == 1)
int oc_connectivity_init_gatt(void);
void oc_connectivity_shutdown_gatt(void);
void oc_send_buffer_gatt(struct oc_message_s *message);
void oc_send_buffer_gatt_mcast(struct oc_message_s *message);
struct oc_message_s *oc_attempt_rx_gatt(void);
#endif

#if (MYNEWT_VAL(OC_TRANSPORT_SERIAL) == 1)
int oc_connectivity_init_serial(void);
void oc_connectivity_shutdown_serial(void);
void oc_send_buffer_serial(struct oc_message_s *message);
struct oc_message_s *oc_attempt_rx_serial(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ADAPTOR_H */

