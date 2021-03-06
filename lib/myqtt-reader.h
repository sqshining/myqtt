/* 
 *  MyQtt: A high performance open source MQTT implementation
 *  Copyright (C) 2016 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build MQTT enabled solutions contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Antonio Suarez Nº 10, 
 *         Edificio Alius A, Despacho 102
 *         Alcalá de Henares 28802 (Madrid)
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/mqtt
 *                        http://www.aspl.es/myqtt
 */
#ifndef __MYQTT_READER_H__
#define __MYQTT_READER_H__

#include <myqtt.h>

void myqtt_reader_watch_listener              (MyQttCtx        * ctx,
						MyQttConn * listener);

void myqtt_reader_watch_connection            (MyQttCtx        * ctx,
					       MyQttConn * connection);

void myqtt_reader_unwatch_connection          (MyQttCtx          * ctx,
					       MyQttConn         * connection,
					       MyQttConnUnwatch    after_unwatch,
					       axlPointer          user_data);

int  myqtt_reader_connections_watched         (MyQttCtx        * ctx);

int  myqtt_reader_run                         (MyQttCtx * ctx);

void myqtt_reader_stop                        (MyQttCtx * ctx);

int  myqtt_reader_notify_change_io_api        (MyQttCtx * ctx);

void myqtt_reader_notify_change_done_io_api   (MyQttCtx * ctx);

void        __myqtt_reader_prepare_wait_reply (MyQttConn * conn, int packet_id, axl_bool peer_ids);

void        __myqtt_reader_remove_wait_reply  (MyQttConn * conn, int packet_id, axl_bool peer_ids);

MyQttMsg  * __myqtt_reader_get_reply          (MyQttConn * conn, int packet_id, int timeout, axl_bool peer_ids);

/**** internal API: do not use this, it may change at any time ****/
typedef void (*MyQttForeachFunc) (MyQttConn * conn, axlPointer user_data);
typedef void (*MyQttForeachFunc3) (MyQttConn * conn, 
				    axlPointer         user_data, 
				    axlPointer         user_data2,
				    axlPointer         user_data3);

MyQttAsyncQueue * myqtt_reader_foreach       (MyQttCtx            * ctx,
					      MyQttForeachFunc      func,
					      axlPointer             user_data);

void               myqtt_reader_foreach_offline (MyQttCtx           * ctx,
						  MyQttForeachFunc3    func,
						  axlPointer            user_data,
						  axlPointer            user_data2,
						  axlPointer            user_data3);

void               myqtt_reader_restart (MyQttCtx * ctx);


/*** private API ***/
void               __myqtt_reader_subscribe (MyQttCtx   * ctx, 
					     const char * client_identifier,
					     MyQttConn  * conn, 
					     char       * topic_filter, 
					     MyQttQos     qos,
					     axl_bool     __is_offline);

void __myqtt_reader_move_offline_to_online  (MyQttCtx * ctx, MyQttConn * conn);

axl_bool myqtt_reader_is_wrong_topic  (const char * topic_filter);

axl_bool myqtt_reader_topic_filter_match (const char * topic_name, const char * topic_filter);


#endif
