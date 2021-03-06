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
#include <myqtt-storage.h>
#include <myqtt-conn-private.h>
#include <myqtt-ctx-private.h>
#include <dirent.h>

/*
 * @internal Allows to report an storage error, giving errno error,
 * and current uid and gid.
 */
void __myqtt_storage_error_report (MyQttCtx * ctx, const char * format, ...)
{
	char * error_msg;
	va_list args;
	
	/* open stdargs */
	va_start (args, format);
	error_msg = axl_stream_strdup_printfv (format, args);
	va_end (args);

	myqtt_log (MYQTT_LEVEL_CRITICAL, "STORAGE ERROR: %s, error was: %s, errno=%d, uid=%d, gid=%d", error_msg, myqtt_errno_get_error (errno), errno, getuid (), getgid ());
	axl_free (error_msg);
	return;
}


/** 
 * \defgroup myqtt_storage MyQtt Storage: Plugable storage API 
 */

/** 
 * \addtogroup myqtt_storage
 * @{
 */

int __myqtt_storage_strpos (MyQttCtx * ctx, const char * string, char item)
{
	int iterator = 0;
	while (string && string[iterator]) {

		if (string[iterator] == item)
			return iterator;

		/* next position */
		iterator++;
	}

	return -1;
}

int __myqtt_storage_check (MyQttCtx * ctx, const char * client_identifier, axl_bool check_tp, const char * topic_filter)
{
	int topic_filter_len = 0;

	/* check 1 */
	if (ctx == NULL || (check_tp && topic_filter == NULL)) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Wrong parameters received, some of them are null (ctx: %p, client_identifier: %p, topic_filter: %p",
			   ctx, client_identifier, topic_filter);
		return axl_false;
	}

	/* check 2 */
	if (client_identifier == NULL || strlen (client_identifier) == 0) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Client identifier is not defined or it is empty");
		return axl_false;
	}

	/* check 3 */
	if (check_tp) {
		topic_filter_len = strlen (topic_filter);
		if (topic_filter_len == 0) {
			myqtt_log (MYQTT_LEVEL_CRITICAL, "Empty topic filter received");
			return axl_false;
		}

		return topic_filter_len;
	} /* end if */

	return axl_true; /* everything ok */
}

axl_bool __myqtt_storage_init_base_storage (MyQttCtx * ctx)
{
	char       * env;
	char       * full_path;

	/* if path is defined and exists, report ok */
	if (ctx->storage_path && myqtt_support_file_test (ctx->storage_path, FILE_EXISTS | FILE_IS_DIR))
		return axl_true;

	myqtt_mutex_lock (&ctx->ref_mutex);
	if (! ctx->storage_path) {
		env = myqtt_support_getenv ("HOME");
		if (env) {
			ctx->storage_path = myqtt_support_build_filename (env, ".myqtt-storage", NULL);
			axl_free (env);
		} else {
			ctx->storage_path = axl_strdup (".myqtt-storage");
		}
		
		/* set default hash size for storage path */
		ctx->storage_path_hash_size = 4096;
	} /* end if */

	/* if reached this point without having storage dir defined fail */
	if (! ctx->storage_path) {
		myqtt_mutex_unlock (&ctx->ref_mutex);
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to allocate/define storage directory for the provided context");
		return axl_false;
	}  /* end if */

	/* create base storage path directory */
	if (! myqtt_support_file_test (ctx->storage_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (myqtt_mkdir (ctx, ctx->storage_path, 0700)) {
			/* release */
			myqtt_mutex_unlock (&ctx->ref_mutex);

			__myqtt_storage_error_report (ctx, "Unable to create storage directory %s", ctx->storage_path);
			return axl_false;
		} /* end if */
	} /* end if */
	
	/* create retained directory */
	full_path = myqtt_support_build_filename (ctx->storage_path, "retained", NULL);
	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (myqtt_mkdir (ctx, full_path, 0700)) {
			/* release */
			myqtt_mutex_unlock (&ctx->ref_mutex);

			__myqtt_storage_error_report (ctx, "Unable to create storage directory %s for retained messages", full_path);
			axl_free (full_path);
			return axl_false;
		} /* end if */
	} /* end if */
	axl_free (full_path);
	
	myqtt_mutex_unlock (&ctx->ref_mutex);

	/* reached ok status here */
	return axl_true;
}

/** 
 * @brief Offline storage initialization for the provided client identifier.
 *
 * See \ref myqtt_storage_init for more information.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifier to initialize storage. 
 *
 * @param storage Part of the storage to initialize.
 *
 * @return If the function is not able to create default storage, the
 * function will fail, otherwise axl_true is returned. The function
 * also returns axl_false in the case client_id is NULL or empty or
 * the context is NULL too.
 */
axl_bool myqtt_storage_init_offline (MyQttCtx * ctx, const char * client_identifier, MyQttStorage storage)
{
	char       * full_path;
	mode_t       umask_mode;

	/* check input parameters */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0)
		return axl_false;

	/* get previous umask and set a secure one by default during operations */
	umask_mode = umask (0077);

	/* lock during operation during this global configuration */
	if (! __myqtt_storage_init_base_storage (ctx)) {
		/* restore umask */
		umask (umask_mode);

		return axl_false;
	} /* end if */

	/* lock during check */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, NULL);
	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (myqtt_mkdir (ctx, full_path, 0700)) {
			/* restore umask */
			umask (umask_mode);

			__myqtt_storage_error_report (ctx, "Unable to create storage directory %s", full_path);
			axl_free (full_path);
			return axl_false;
		} /* end if */
	} /* end if */

	myqtt_log (MYQTT_LEVEL_DEBUG, "Storage dir created at: %s", full_path);
	axl_free (full_path);

	/* now create message directory, subs and will */
	if ((storage & MYQTT_STORAGE_MSGS) == MYQTT_STORAGE_MSGS) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (myqtt_mkdir (ctx, full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);

				__myqtt_storage_error_report (ctx, "Unable to create storage directory %s for messages", full_path);
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* subs */
	if ((storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "subs", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (myqtt_mkdir (ctx, full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);

				__myqtt_storage_error_report (ctx, "Unable to create storage directory %s for subscriptions", full_path);
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* will */
	if ((storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "will", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (myqtt_mkdir (ctx, full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);

				__myqtt_storage_error_report (ctx, "Unable to create storage directory %s for messages", full_path);
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* pkgids */
	if ((storage & MYQTT_STORAGE_PKGIDS) == MYQTT_STORAGE_PKGIDS) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (myqtt_mkdir (ctx, full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);

				__myqtt_storage_error_report (ctx, "Unable to create storage directory %s for messages", full_path);
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* restore umask */
	umask (umask_mode);

	return axl_true;
}


/** 
 * @brief Inits storage service for the provided client id.
 *
 * You can use \ref myqtt_storage_set_path to change default
 * location. If nothing is provided on the previous function, the
 * following locations are used to handle session storage:
 *
 * - ${HOME}/.myqtt-storage (if HOME variable is defined).
 * - .myqtt-storage
 * 
 *
 * @param ctx The context where the operation takes place. Cannot be NULL.
 *
 * @param conn The connection with a client identifier to init storage
 * services. Cannot be NULL
 *
 * @param storage Storage configuration to initialize
 *
 * @return If the function is not able to create default storage, the
 * function will fail, otherwise axl_true is returned. The function
 * also returns axl_false in the case client_id is NULL or empty or
 * the context is NULL too.
 */
axl_bool myqtt_storage_init (MyQttCtx * ctx, MyQttConn * conn, MyQttStorage storage)
{
	axl_bool result;

	/* check input parameters */
	if (ctx == NULL || conn == NULL || conn->client_identifier == NULL || strlen (conn->client_identifier) == 0)
		return axl_false;

	/* record storage was initilized */
	if ((storage & conn->myqtt_storage_init) == storage)
		return axl_true;

	/* create base storage path directory */
	myqtt_mutex_lock (&conn->op_mutex);

	/* check again initialization */
	if ((storage & conn->myqtt_storage_init) == storage) {

		/* already initialized */
		/* create base storage path directory */
		myqtt_mutex_unlock (&conn->op_mutex);

		return axl_true;
	} /* end if */

	/* call to offline implementation */
	result = myqtt_storage_init_offline (ctx, conn->client_identifier, storage);

	/* remember result only on ok event */
	if (result) {
		/* record storage was initilized */
		conn->myqtt_storage_init |= storage;
	} /* end if */
	/* release */
	myqtt_mutex_unlock (&conn->op_mutex);
	
	return result;
}

/** 
 * @brief Allows to clear current session storage associated to the provided connection.
 *
 * Cleaning session storage includes cleaning subscriptions, pending
 * messages and will configuration (if any).
 *
 * @param ctx The context where the operation takes place
 *
 * @param conn The connection with a client identifier to select the storage to clear
 *
 * @param storage The storage to remove. 
 *
 * @return axl_true If the operation completes, otherwise axl_false is
 * returned when a failure is found or parameters received aren't well defined.
 */
axl_bool           myqtt_storage_clear            (MyQttCtx      * ctx,
						   MyQttConn     * conn,
						   MyQttStorage    storage)
{
	axl_bool result;

	/* check input parameters */
	if (ctx == NULL || conn == NULL || conn->client_identifier == NULL || strlen (conn->client_identifier) == 0)
		return axl_false;

	/* create base storage path directory */
	myqtt_mutex_lock (&conn->op_mutex);

	/* call to offline implementation */
	result = myqtt_storage_clear_offline (ctx, conn->client_identifier, storage);

	/* release */
	myqtt_mutex_unlock (&conn->op_mutex);

	return result;
}

axl_bool __myqtt_storage_remove_files_from_dir (MyQttCtx * ctx, const char * clear_files_in_dir)
{
	struct dirent * entry;
	DIR           * dir;
	char          * full_path;

	/* if directory does not exists, just say ok and leave it like
	 * this */
	if (! myqtt_support_file_test (clear_files_in_dir, FILE_EXISTS | FILE_IS_DIR))
		return axl_true;

	/* open dir */
	dir = opendir (clear_files_in_dir);
	if (dir == NULL) {
		__myqtt_storage_error_report (ctx, "Failed to open directory %s for file deletion", clear_files_in_dir);
		return axl_false;
	}
	
	/* get first entry */
	entry = readdir (dir);
	while (entry) {
		/* skip known directories we are not interested in */
		if (axl_cmp (entry->d_name, ".") || axl_cmp (entry->d_name, "..")) {
			/* get next entry */
			entry = readdir (dir);
			continue;
		} /* end if */

		/* check if we are talking about a file */
		full_path = myqtt_support_build_filename (clear_files_in_dir, entry->d_name, NULL);
		if (myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			unlink (full_path);
		} else if (myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR))
			__myqtt_storage_remove_files_from_dir (ctx, full_path);
		axl_free (full_path);
		
		/* get next entry */
		entry = readdir (dir);
	} /* end if */

	closedir (dir);

	return axl_true;
}

/** 
 * @brief Clears the storage associated to the provided client_identifier
 *
 * @param ctx The context where the operation will take place.
 *
 * @param client_identifier The client identifier for which the storage will be cleared.
 *
 * @param storage The part of the storage that have to be cleared.
 *
 * @return axl_true in the case clear operation took place without
 * errors, otherwise axl_false is returned.
 */
axl_bool myqtt_storage_clear_offline    (MyQttCtx      * ctx, 
					 const char    * client_identifier, 
					 MyQttStorage    storage)
{
	char      * full_path;
	axl_bool    result;

	/* check input parameters */
	if (ctx == NULL || client_identifier == NULL)
		return axl_false;

	/* lock during check */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, NULL);
	result    = myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR);

	/* release full path and check result */
	axl_free (full_path);
	if (! result) 
		return axl_true;

	/* now create message directory, subs and will */
	if ((storage & MYQTT_STORAGE_MSGS) == MYQTT_STORAGE_MSGS || 
	    (storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", NULL);
		result    = __myqtt_storage_remove_files_from_dir (ctx, full_path);

		/* release full path */
		axl_free (full_path);
	} /* end if */

	/* subs */
	if ((storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "subs", NULL);
		result    = __myqtt_storage_remove_files_from_dir (ctx, full_path);

		/* release full path */
		axl_free (full_path);

	} /* end if */

	/* will */
	if ((storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "will", NULL);
		result    = __myqtt_storage_remove_files_from_dir (ctx, full_path);

		/* release full path */
		axl_free (full_path);

	} /* end if */

	/* pkgids */
	if ((storage & MYQTT_STORAGE_PKGIDS) == MYQTT_STORAGE_PKGIDS || (storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", NULL);
		result    = __myqtt_storage_remove_files_from_dir (ctx, full_path);

		/* release full path */
		axl_free (full_path);

	} /* end if */

	return axl_true;
}

void      __myqtt_storage_get_values_from_file_name (MyQttCtx * ctx, const char * file_name, 
						     int * packet_id, int * size, int * qos)
{
	int pos;
	int desp = 0;

	/* get packet_id */
	pos          = 0;
	myqtt_log (MYQTT_LEVEL_DEBUG, "Getting packet_id from position: %s", file_name + desp + 1);
	(*packet_id) = __myqtt_storage_get_size_from_file_name (ctx, file_name, &pos);
	desp         = pos;
	
	/* get size */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Getting size from position: %s", file_name + desp + 1);
	(*size)      = __myqtt_storage_get_size_from_file_name (ctx, file_name + desp + 1, &pos);
	desp        += (pos + 1);
	
	/* get qos */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Getting qos from position: %s", file_name + desp + 1);
	(*qos)       = __myqtt_storage_get_size_from_file_name (ctx, file_name + desp + 1, &pos);
	desp        += (pos + 1);

	return;
}

int      __myqtt_storage_get_size_from_file_name (MyQttCtx * ctx, const char * file_name, int * position)
{
	int  iterator = 0;
	char buffer[10];

	/* clear buffer */
	memset (buffer, 0, 10);
	while (file_name[iterator] && iterator < 10) {
		if (file_name[iterator] < 48 || file_name[iterator] > 57) {
			if (position)
				(*position) = iterator;
			return myqtt_support_strtod (buffer, NULL);
		}

		buffer[iterator] = file_name[iterator];
		iterator++;
	}

	/* unable to identify value */
	return -1;
}

axl_bool __myqtt_storage_read_content_into_reference (MyQttCtx * ctx, const char * file_path, unsigned char ** app_msg, int * app_msg_size)
{
	FILE         * handle;
	struct stat    stat_ref;

	if (! app_msg)
		return axl_false;

	/* clear received references */
	*app_msg_size = -1;
	*app_msg      = NULL;

	/* get size */
	if (stat (file_path, &stat_ref) != 0) {
		__myqtt_storage_error_report (ctx, "Failed to get stat(%s)", file_path);
		return axl_false;
	} /* end if */

	if (stat_ref.st_size > MYQTT_MAX_MSG_SIZE) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to read content from %s, it has more size (%d) than allowed (%d)",
			   file_path, stat_ref.st_size);
		return axl_false;
	}
	
	/* report st size */
	if (app_msg_size)
		(*app_msg_size) = stat_ref.st_size;

	/* open file */
	handle = fopen (file_path, "r");
	if (! handle) {
		__myqtt_storage_error_report (ctx, "Unable to open file at %s", file_path);
		return axl_false;
	}
	
	/* read app message into app_msg */
	(*app_msg) = axl_new (unsigned char, stat_ref.st_size + 1);
	if ((*app_msg) == NULL) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Failed to allocate %d bytes to recover message", stat_ref.st_size);
		fclose (handle);
		return axl_false;
	} /* end if */

	/* read entire content */
	if (fread (*app_msg, 1, stat_ref.st_size, handle) != stat_ref.st_size) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "fread (%p, 1, %d, %s) operation failed, expected to read %d, error was: %s", 
			   *app_msg, stat_ref.st_size, file_path, stat_ref.st_size, myqtt_errno_get_error (errno));

		fclose (handle);
		axl_free (*app_msg);
		return axl_false;
	} /* end if */

	fclose (handle);
	return axl_true;
}

axl_bool __myqtt_storage_sub_exists (MyQttCtx         * ctx, 
				     const char       * full_path, 
				     const char       * topic_filter, 
				     int                topic_filter_len, 
				     /* do not request for this parameter */
				     /* MyQttQos     requested_qos,  */
				     axl_bool           remove_if_found, 
				     axl_bool           remove_msg_if_found,
				     MyQttQos         * qos,
				     unsigned char   ** app_msg,
				     int              * app_msg_size)
{
	struct dirent * entry;
	DIR           * sub_dir;
	int             sub_size;
	char            buffer[4096];
	int             desp;
	int             bytes_read;
	FILE          * handle;
	char          * aux_path, * aux_path2;
	axl_bool        matches;
	int             pos;
	axl_bool        result = axl_false;

	sub_dir = opendir (full_path);
	if (sub_dir == NULL) {
		/* myqtt_log (MYQTT_LEVEL_CRITICAL, "Failed to open directory %s for inspection, error was: %s", full_path, myqtt_errno_get_error (errno)); */
		return axl_false;
	} /* end if */

	/* clear buffer */
	memset (buffer, 0, 4096);

	entry = readdir (sub_dir);
	while (entry) {
		/* skip known directories we are not interested in */
		if (axl_cmp (entry->d_name, ".") || axl_cmp (entry->d_name, "..")) {
			/* get next entry */
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* skip .msg files */
		if (strstr (entry->d_name, ".msg")) {
			/* get next entry */
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* get subscription size */
		sub_size = __myqtt_storage_get_size_from_file_name (ctx, entry->d_name, &pos);

		/* report qos if requested */
		if (qos) 
			(*qos) = __myqtt_storage_get_size_from_file_name (ctx, entry->d_name + pos + 1, NULL);

		if (sub_size == topic_filter_len) {
			/* ok, found a size match, try now to check content */
			aux_path  = myqtt_support_build_filename (full_path, entry->d_name, NULL);

			/* open the file */
			handle    = fopen (aux_path, "r");
			if (handle == NULL) {
				/* get next entry */
				entry = readdir (sub_dir);
				axl_free (aux_path);
				continue;
			} /* end if */


			desp    = 0;
			matches = axl_true;
			while (desp < topic_filter_len) {

				/* read content from file into the buffer to do a partial check */
				bytes_read = fread (buffer, 1, 4096, handle);

				/* do a partial check */
				if (! axl_memcmp (buffer, topic_filter + desp, bytes_read)) {
					/* mismatch found, this is not the file */
					matches = axl_false;
					break;
				} /* end if */
				
				desp += bytes_read;
			} /* end while */

			/* close file handle anyway */
			fclose (handle);

			myqtt_log (MYQTT_LEVEL_DEBUG, "Checking file %s, matches=%d", aux_path, matches);

			/* if it matched, apply final actions */
			if (matches) {

				/* remove subscription */
				if (remove_if_found) 
					unlink (aux_path);

				/* check to remove .msg if requested */
				if (remove_msg_if_found || app_msg) {
					aux_path2 = axl_strdup_printf ("%s.msg", aux_path);

					/* read content into reference */
					if (app_msg) {
						/* myqtt_log (MYQTT_LEVEL_DEBUG, "Calling to recover message %s", aux_path2); */
						result = __myqtt_storage_read_content_into_reference (ctx, aux_path2, app_msg, app_msg_size);
						myqtt_log (MYQTT_LEVEL_DEBUG, "Calling to recover message %s, status: %d", aux_path2, result);
					}

					unlink (aux_path2);
					axl_free (aux_path2);

					if (app_msg) {
						if (! result && app_msg) {
							/* nullify app message references */
							(*app_msg) = NULL;
							(*app_msg_size) = 0;
						} /* end if */

						axl_free (aux_path);
						closedir (sub_dir);
						return result;
					} /* end if */
						
				} /* end if */

				axl_free (aux_path);
				closedir (sub_dir);
				return axl_true;
			} /* end if */

			/* reached this point, we have found the file */
			axl_free (aux_path);

		} /* end if */


		/* get next entry */
		entry = readdir (sub_dir);
	}

	closedir (sub_dir);

	return axl_false;
}

/** 
 * @brief Function to record subscription for the provided client at
 * the current storage.
 *
 * @param ctx The context where the storage operation takes place.
 *
 * @param conn The connection for which the subscription will be stored.
 *
 * @param topic_filter The subscription's topic filter
 *
 * @param requested_qos The QoS requested by the application level.
 *
 * @return axl_true in the case the operation was completed otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_sub (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter, MyQttQos requested_qos)
{
	if (conn == NULL)
		return axl_false;

	/* call offline function */
	return myqtt_storage_sub_offline (ctx, conn->client_identifier, topic_filter, requested_qos);
}

/** 
 * @brief Offline function to record subscription for the provided
 * client identifer at the current storage.
 *
 * @param ctx The context where the storage operation takes place.
 *
 * @param client_identifier The client identifier where the store the subscription.
 *
 * @param topic_filter The subscription's topic filter
 *
 * @param requested_qos The QoS requested by the application level.
 *
 * @return axl_true in the case the operation was completed otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_sub_offline      (MyQttCtx      * ctx, 
					 const char    * client_identifier,
					 const char    * topic_filter, 
					 MyQttQos        requested_qos)
{
	char             * full_path;
	char             * hash_value;
	char             * path_item;
	struct timeval     stamp;
	int                topic_filter_len;
	int                written;
	FILE             * sub_file;

	/* check input parameters */
	topic_filter_len = __myqtt_storage_check (ctx, client_identifier, axl_true, topic_filter);
	if (! topic_filter_len)
		return axl_false;

	if (ctx == NULL || ctx->storage_path_hash_size == 0)
		return axl_false;

	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_filter) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return axl_false;

	/* now create message directory */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "subs", hash_value, NULL);
	if (full_path == NULL) {
		axl_free (hash_value);
		return axl_false;
	} /* end if */

	/* create parent directory if it wasn't created */
	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (myqtt_mkdir (ctx, full_path, 0700)) {
			/* get log */
			__myqtt_storage_error_report (ctx, "Unable to create topic filter directory to store subscription: %s", full_path);

			axl_free (full_path);
			axl_free (hash_value);
			return axl_false;
		} /* end if */
	} else {
		/* directory exists, check if the subscription is
		 * already on the disk */
		if (__myqtt_storage_sub_exists (ctx, full_path, topic_filter, topic_filter_len, axl_false, axl_false, NULL, NULL, NULL)) {
			axl_free (full_path);
			axl_free (hash_value);
			return axl_true;
		} /* end if */

	} /* end if */
	axl_free (full_path);

	/* now storage subscription */
	gettimeofday (&stamp, NULL);
	path_item = axl_strdup_printf ("%d-%d-%d-%d-%d", topic_filter_len, requested_qos, hash_value, stamp.tv_sec, stamp.tv_usec);

	if (path_item == NULL) {
		axl_free (hash_value);
		return axl_false;
	} /* end if */

	/* build full path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "subs", hash_value, path_item, NULL);
	axl_free (path_item);
	axl_free (hash_value);

	if (full_path == NULL) 
		return axl_false;

	/* open file and release path */
	sub_file = fopen (full_path, "w");
	if (sub_file == NULL) {
		/* report log */
		__myqtt_storage_error_report (ctx, "Unable to save subscription, failed to write to file '%s'", full_path);
		axl_free (full_path);
		return axl_false;
	} /* end if */
	axl_free (full_path);

	written = fwrite (topic_filter, 1, topic_filter_len, sub_file);
	if (written != topic_filter_len) {
		/* report log */
		__myqtt_storage_error_report (ctx, "fwrite() call failed to write expected bytes (%d), written (%d)", topic_filter_len, written);
		return axl_false;
	} /* end if */

	fclose (sub_file);

	return axl_true;
}

/** 
 * @brief Allows to check if the provided topic_filter is already subscribed for the provided connection and optionally removing it.
 *
 * @param ctx The context where operation takes place.
 *
 * @param conn The connection to check subscription.
 *
 * @param topic_filter The topic filter to check.
 *
 * @param requested_qos Requested QoS for the subscription.
 *
 * @param remove_if_found Remove the subscription if found.
 *
 * @return axl_true if the subscription oepration was completed,
 * otherwise axl_false is reported.
 */
axl_bool myqtt_storage_sub_exists_common (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter, MyQttQos requested_qos, axl_bool remove_if_found)
{
	int    topic_filter_len;
	char * full_path;
	char * hash_value;

	if (ctx == NULL || ctx->storage_path_hash_size == 0)
		return axl_false;

	/* check input parameters */
	topic_filter_len = __myqtt_storage_check (ctx, conn->client_identifier, axl_true, topic_filter);
	if (! topic_filter_len)
		return axl_false;

	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_filter) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return axl_false;

	/* now create message directory */
	full_path = myqtt_support_build_filename (ctx->storage_path, conn->client_identifier, "subs", hash_value, NULL);
	axl_free (hash_value);
	if (full_path == NULL) {
		return axl_false;
	} /* end if */

	/* call to check subscription in the provided directory */
	if (__myqtt_storage_sub_exists (ctx, full_path, topic_filter, topic_filter_len, remove_if_found, axl_false, NULL, NULL, NULL)) {
		axl_free (full_path);
		return axl_true;
	} /* end if */

	axl_free (full_path);

	return axl_false;
}

/** 
 * @brief Allows to check if the provided topic_filter is already
 * subscribed for the provided connection.
 *
 * @param ctx The context where operation takes place.
 *
 * @param conn The connection to check subscription.
 *
 * @param topic_filter The topic filter to check.
 *
 * @return axl_true if the subscription oepration was completed,
 * otherwise axl_false is reported.
 */
axl_bool myqtt_storage_sub_exists (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter)
{
	/* call to check if it exists but without removing without caring about qos */
	return myqtt_storage_sub_exists_common (ctx, conn, topic_filter, 0, axl_false);
}

void __myqtt_storage_sub_conn_register (MyQttCtx * ctx, const char * client_identifier, MyQttConn * conn, const char * file_name, const char * full_path, axl_bool __is_offline)
{
	/* get qos from file name */
	int        pos           = 0;
	int        size          = __myqtt_storage_get_size_from_file_name (ctx, file_name, &pos);
	MyQttQos   qos           = MYQTT_QOS_0;
	char     * topic_filter;
	FILE     * _file;

	/* get qos */
	if (pos >= (strlen (file_name) + 1))
		return;

	qos = __myqtt_storage_get_size_from_file_name (ctx, file_name + pos + 1, NULL);
	topic_filter = axl_new (char, size + 1);
	if (topic_filter == NULL)
		return;

	_file = fopen (full_path, "r");
	if (_file == NULL) {
		axl_free (topic_filter);
		return;
	} /* end if */

	if (fread (topic_filter, 1, size, _file) != size) {
		fclose (_file);
		axl_free (topic_filter);
		return;
	} /* end if */
	fclose (_file);

	/* call to register */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Recovering subs for %s qos=%d sub=%s", client_identifier, qos, topic_filter);
	/* printf ("**\n** __myqtt_storage_sub_conn_register : calling to __myqtt_reader_subscribe (ctx=%p, conn=%p)..\n**\n", ctx, conn); */
	__myqtt_reader_subscribe (ctx, client_identifier, conn, topic_filter, qos, __is_offline);

	/* it is not required to release topic_filter here, that
	 * reference is not owned by __myqtt_reader_subscribe */

	return;
}

int __myqtt_storage_sub_count_aux (MyQttCtx * ctx, const char * client_identifier, MyQttConn * conn, const char * aux_path, axl_bool __register, axl_bool __is_offline)
{
	DIR           * files;
	struct dirent * entry;
	char          * full_path;
	int             count = 0;

	files = opendir (aux_path);
	entry = readdir (files);
	while (files && entry) {

		/* get next entry and skip those we are not interested in */
		if (axl_cmp (".", entry->d_name) || axl_cmp ("..", entry->d_name)) {
			entry = readdir (files);
			continue;
		} /* end if */
		
		/* build path */
#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_REG) == DT_REG)
			count ++;

		if (__register) {
			/* register subscription */
			full_path = myqtt_support_build_filename (aux_path, entry->d_name, NULL);
			__myqtt_storage_sub_conn_register (ctx, client_identifier, conn, entry->d_name, full_path, __is_offline);
			axl_free (full_path);
		} /* end if */
#else 
		full_path = myqtt_support_build_filename (aux_path, entry->d_name, NULL);
		if (myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			count ++;
		} /* end if */

		/* call to register */
		if (__register)
			__myqtt_storage_sub_conn_register (ctx, conn, entry->d_name, full_path, __is_offline);

		axl_free (full_path);
#endif
		/* next file */
		entry = readdir (files);
	}

	closedir (files);

	return count;
}

/** 
 * @internal Function to iterate over all subscriptions to restore
 * connection state.
 *
 * @param ctx The context where the operation will take place
 *
 * @param conn The connection where the operation will be implemented.
 */
int __myqtt_storage_iteration (MyQttCtx * ctx, const char * client_identifier, MyQttConn * conn, axl_bool __register, axl_bool __is_offline) {

	char          * full_path;
	char          * aux_path;
	DIR           * sub_dir;
	struct dirent * entry;
	int             total = 0;

	/* check input parameters */
	if (! __myqtt_storage_check (ctx, client_identifier, axl_false, NULL))
		return axl_false;

	/* get full path to subscriptions */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "subs", NULL);
	if (full_path == NULL) 
		return axl_false; /* allocation failure */

	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		axl_free (full_path);
		return axl_false; /* directory do not exists */
	} /* end if */

	/* try to open path */
	sub_dir = opendir (full_path);
	if (sub_dir == NULL) {
		__myqtt_storage_error_report (ctx, "Unable to open %s", full_path);
		axl_free (full_path);
		return axl_false;
	} /* end if */
	entry   = readdir (sub_dir);
	while (sub_dir && entry) {

		/* get next entry and skip those we are not interested in */
		if (axl_cmp (".", entry->d_name) || axl_cmp ("..", entry->d_name)) {
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* check if it is a directory */
#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_DIR) == DT_DIR) {
			/* count subscriptions */
			aux_path  = myqtt_support_build_filename (full_path, entry->d_name, NULL);
			total    += __myqtt_storage_sub_count_aux (ctx, client_identifier, conn, aux_path, __register, __is_offline);
			axl_free (aux_path);
		}
#else 
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_DIR)) {
			/* count subscriptions */
			total += __myqtt_storage_sub_count_aux (ctx, conn, aux_path, __register, __is_offline);
		}
		axl_free (aux_path);
#endif

		/* get next entry */
		entry   = readdir (sub_dir);
	}

	closedir (sub_dir);
	axl_free (full_path);

	if (__register)
		return total > 0 ? total : __register;

	return total;
}

/** 
 * @brief Allows to get current number of subscriptions registered on
 * the storage for the provided client identifier.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifier to get subscription count from.
 *
 * @return Number of subscriptions.
 */
int      myqtt_storage_sub_count_offline (MyQttCtx      * ctx, 
					  const char    * client_identifier)
{
	return __myqtt_storage_iteration (ctx, client_identifier, NULL, /* register */ axl_false, /* offline */ axl_true);
}

/** 
 * @brief Allows to get current number of subscriptions registered on
 * the storage for the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn Connection to get subscription count from.
 *
 * @return Number of subscriptions.
 */
int      myqtt_storage_sub_count (MyQttCtx * ctx, MyQttConn * conn)
{
	if (conn == NULL)
		return 0;

	/* call to iterate and count */
	return __myqtt_storage_iteration (ctx, conn->client_identifier, NULL, /* register */ axl_false, /* offline */ axl_false);
}

/** 
 * @brief Function to unsubscribe the provided topic filter on the
 * provided connection.
 *
 * @param ctx The context where the operation will take place.
 *
 * @param conn The connection for which the subscription will be stored.
 *
 * @param topic_filter The unsubscription' topic filter
 *
 * @return axl_true in the case the operation was completed otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_unsub (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter) 
{
	/* call to remove subscription */
	return myqtt_storage_sub_exists_common (ctx, conn, topic_filter, 0, axl_true);
}

/** 
 * @brief Allows to storage the provided message on the local session
 * storage associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client id for which the operation is requested.
 *
 * @param packet_id The packet id associated to the message.
 *
 * @param qos The QoS of the operation requested.
 *
 * @param app_msg The application message to store.
 *
 * @param app_msg_size The size of the application message to store.
 *
 * @return The function return NULL on failure and a handler that
 * points to the message stored.
 */
axlPointer myqtt_storage_store_msg_offline (MyQttCtx      * ctx, 
					    const char    * client_identifier,
					    int             packet_id, 
					    MyQttQos        qos, 
					    unsigned char * app_msg, 
					    int             app_msg_size)
{
	char            * full_path;
	char            * ref;
	struct timeval    stamp;
	FILE            * handle;

	/* check input values:
	 *
	 * don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0 || packet_id < 0 || app_msg_size <= 0 || app_msg == NULL)
		return NULL;

	if (ctx->on_store && !(qos && MYQTT_QOS_SKIP_STOREAGE_NOTIFY)) {
		/* call to check if we can store the message */
		if (! ctx->on_store (ctx, NULL, client_identifier, packet_id, qos, app_msg, app_msg_size, ctx->on_store_data))
			return NULL;
	} /* end if */

	/* call to init message store */
	if (! myqtt_storage_init_offline (ctx, client_identifier, MYQTT_STORAGE_MSGS))
		return NULL;

	/* build path */
	gettimeofday (&stamp, NULL);
	ref       = axl_strdup_printf ("%d-%d-%d-%d-%d", packet_id, app_msg_size, qos, stamp.tv_sec, stamp.tv_usec);
	if (! ref)
		return NULL;
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", ref, NULL);
	axl_free (ref);
	if (! full_path) 
		return NULL;

	/* save file */
	handle = fopen (full_path, "w");
	if (! handle) {
		/* report failure */
		__myqtt_storage_error_report (ctx, "Failed to store message at %s", full_path);
		axl_free (full_path);
		return NULL;
	} /* end if */
	
	if (fwrite (app_msg, 1, app_msg_size, handle) != app_msg_size) {
		__myqtt_storage_error_report (ctx, "Failed to storage message at %s", full_path);
		fclose (handle);
		axl_free (full_path);
		return NULL;
	} /* end if */

	/* message saved */
	fclose (handle);
	return full_path;	
}

/** 
 * @brief Allows to storage the provided message on the local session
 * storage associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection to select local session storage.
 *
 * @param packet_id The packet id associated to the message.
 *
 * @param qos QoS of the message to be stored.
 *
 * @param app_msg The application message to store.
 *
 * @param app_msg_size The size of the application message to store.
 *
 * @return The function return NULL on failure and a handler that
 * points to the message stored.
 */
axlPointer myqtt_storage_store_msg   (MyQttCtx * ctx, MyQttConn * conn, int packet_id, MyQttQos qos, unsigned char * app_msg, int app_msg_size)
{
	/* avoid segfault when conn reference is NULL */
	if (conn == NULL)
		return axl_false; 

	if (ctx->on_store) {
		/* call to check if we can store the message */
		if (! ctx->on_store (ctx, conn, conn->client_identifier, packet_id, qos, app_msg, app_msg_size, ctx->on_store_data))
			return NULL;
	} /* end if */

	return myqtt_storage_store_msg_offline (ctx, conn->client_identifier, packet_id, qos | MYQTT_QOS_SKIP_STOREAGE_NOTIFY, app_msg, app_msg_size);
}

/** 
 * @brief Allows to release the provided message on the local session
 * storage associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection to select local session storage.
 *
 * @param handle Reference to the message as returned by \ref myqtt_storage_store_msg
 *
 * @param app_msg The application message to be released.
 *
 * @param app_msg_size Application message size to be released.
 *
 * @return axl_true if the message was successfully stored, otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_release_msg   (MyQttCtx      * ctx, 
				      MyQttConn     * conn, 
				      axlPointer      handle,
				      unsigned char * app_msg,
				      int             app_msg_size)
{
	MyQttQos qos;
	int      packet_id;
	int      pos;

	/* check input values */
	if (ctx == NULL || conn == NULL)
		return axl_false;

	/* call to init message store */
	if (! myqtt_storage_init (ctx, conn, MYQTT_STORAGE_MSGS))
		return axl_false;

	/* release the message */
	if (handle) {
		/* check and call on release message */
		if (ctx->on_release) {
			/* get packet id */
			packet_id = __myqtt_storage_get_size_from_file_name (ctx, handle, NULL);
			pos       = __myqtt_storage_strpos (ctx, handle, '-');

			/* get next post to skip over app msg size */
			pos       = __myqtt_storage_strpos (ctx, handle + pos + 1, '-');

			/* get qos */
			qos       = __myqtt_storage_get_size_from_file_name (ctx, handle + pos + 1, NULL);

			/* call to notify release */
			ctx->on_release (ctx, conn, conn->client_identifier, packet_id, qos, app_msg, app_msg_size, ctx->on_release_data);

		} /* end if */

		unlink ((const char *) handle);
		axl_free ((char *) handle);
	} /* end if */

	return axl_true;
}

/** 
 * @brief Allows to store retain message for the provided topic name
 * so every new subscription on that topic will receive that message.
 *
 * The function will replace the previous retained message. If you
 * want to remove an existing message call to \ref myqtt_storage_retain_msg_release
 *
 * @param ctx The context where the operation takes place.
 *
 * @param topic_name The topic name for the message and at the same
 * time the subscription for which the message will be stored as
 * retained.
 *
 * @param qos The app message qos.
 *
 * @param app_msg The application message to store.
 *
 * @param app_msg_size Application message size.
 *
 *
 * @return axl_true in the case retain message was configured without errors, otherwise axl_false is returned.
 */
axl_bool       myqtt_storage_retain_msg_set (MyQttCtx            * ctx,
					     const char          * topic_name,
					     MyQttQos              qos,
					     const unsigned char * app_msg,
					     int                   app_msg_size)
{
	char            * hash_value;
	int               topic_filter_len;
	char            * full_path;
	char            * aux_path;
	char            * path_item;
	struct timeval    stamp;
	FILE            * handle;

	if (ctx == NULL || topic_name == NULL || app_msg_size < 0)
		return axl_false;

	if (ctx == NULL || ctx->storage_path_hash_size == 0)
		return axl_false;

	/* get topic filter len */
	topic_filter_len = strlen (topic_name);
	if (topic_filter_len == 0)
		return axl_false;

	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_name) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return axl_false;

	/* call to check subscription in the provided directory. If it exists, remove it */	
	full_path = myqtt_support_build_filename (ctx->storage_path, "retained", hash_value, NULL);
	if (full_path == NULL) {
		axl_free (hash_value);
		return axl_false;
	} /* end if */

	/* remove the retained message if it does exists */
	if (myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		/* directory exists, check to remove previous subscription */
		__myqtt_storage_sub_exists (ctx, full_path, topic_name, strlen (topic_name), axl_true, axl_true, NULL, NULL, NULL);
	} else {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Found path %s does not exists, calling myqtt_mkdir ()", full_path);
		
		/* directory is not present, try to create it */
		if (myqtt_mkdir (ctx, full_path, 0700)) {
			__myqtt_storage_error_report (ctx, "Failed to create directory %s, unable to storage retained message, mkdir() failed", full_path);
			axl_free (hash_value);
			axl_free (full_path);
			return axl_false;
		} /* end if */
	} /* end if */

	/* release base path */
	axl_free (full_path);

	/* now save retained message, first subscription topic and qos */
	gettimeofday (&stamp, NULL);
	path_item = axl_strdup_printf ("%d-%d-%d-%d-%d", topic_filter_len, qos, hash_value, stamp.tv_sec, stamp.tv_usec);
	full_path = myqtt_support_build_filename (ctx->storage_path, "retained", hash_value, path_item, NULL);
	axl_free (path_item);
	axl_free (hash_value);

	/* open handle */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Saving retained message at %s", full_path);
	handle = fopen (full_path, "w");
	if (! handle) {
		__myqtt_storage_error_report (ctx, "Failed to open file %s", full_path);
		axl_free (full_path);
		return axl_false;
	} /* end if */

	/* write subscription */
	if (fwrite (topic_name, 1, topic_filter_len, handle) != topic_filter_len) {
		fclose (handle);
		__myqtt_storage_error_report (ctx, "Unable to store topic filter value at %s", full_path);
		axl_free (full_path);
		return axl_false;
	} /* end if */

	/* close this initial file */
	fclose (handle);
	
	/* now write message content */
	aux_path = axl_strdup_printf ("%s.msg", full_path);
	axl_free (full_path);

	/* open handle */
	handle = fopen (aux_path, "w");
	if (! handle) {
		__myqtt_storage_error_report (ctx, "Failed to open file %s", aux_path);
		axl_free (full_path);
		return axl_false;
	} /* end if */

	/* write subscription */
	if (fwrite (app_msg, 1, app_msg_size, handle) != app_msg_size) {
		fclose (handle);
		__myqtt_storage_error_report (ctx, "Unable to store topic filter value at %s", full_path);
		axl_free (full_path);
		return axl_false;
	}
	fclose (handle);

	axl_free (aux_path);
	
	return axl_true;
}

/** 
 * @brief Allows to release retain message (if any) associated to the
 * provided topic_filter.
 */
void       myqtt_storage_retain_msg_release (MyQttCtx      * ctx,
					     const char    * topic_name)
{
	char            * hash_value;
	int               topic_filter_len;
	char            * full_path;


	if (ctx == NULL || topic_name == NULL)
		return;

	if (ctx == NULL || ctx->storage_path_hash_size == 0)
		return;

	/* get topic filter len */
	topic_filter_len = strlen (topic_name);
	if (topic_filter_len == 0)
		return;
	
	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_name) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return;

	/* call to check subscription in the provided directory. If it exists, remove it */	
	full_path = myqtt_support_build_filename (ctx->storage_path, "retained", hash_value, NULL);
	axl_free (hash_value);
	if (full_path == NULL) 
		return;

	/* remove subscription (well, in fact topic name) and message associated if it exists */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Releasing retained message for subscription %s at %s", topic_name, full_path);
	__myqtt_storage_sub_exists (ctx, full_path, topic_name, strlen (topic_name), axl_true, axl_true, NULL, NULL, NULL);
	axl_free (full_path);

	return;
}

/** 
 * @brief Allows to recover retained message for the provided topic (if any).
 *
 * @param ctx The context where the operation takes place.
 *
 * @param topic_name The topic name for which the retain message recovery will be attempted
 *
 * @param qos A referece to hold recovered qos
 *
 * @param app_msg A referece to hold recovered application message.
 *
 * @param app_msg_size A reference to hold recovered application message size.
 *
 * @return The function reports axl_true when the message was
 * recovered. Otherwise, axl_false is reported.
 */
axl_bool      myqtt_storage_retain_msg_recover (MyQttCtx       * ctx,
						const char     * topic_name,
						MyQttQos       * qos,
						unsigned char ** app_msg,
						int            * app_msg_size)
{
	char            * hash_value;
	int               topic_filter_len;
	char            * full_path;
	axl_bool          result;

	if (ctx == NULL || topic_name == NULL)
		return axl_false;

	/* get topic filter len */
	topic_filter_len = strlen (topic_name);
	if (topic_filter_len == 0)
		return axl_false;

	if (ctx == NULL || ctx->storage_path_hash_size == 0)
		return axl_false;

	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_name) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return axl_false;
	
	/* call to check subscription in the provided directory. If it exists, remove it */	
	full_path = myqtt_support_build_filename (ctx->storage_path, "retained", hash_value, NULL);
	axl_free (hash_value);
	if (full_path == NULL) 
		return axl_false;
	
	/* remove subscription (well, in fact topic name) and message associated if it exists */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Recovering retained message for subscription %s at %s", topic_name, full_path);
	result = __myqtt_storage_sub_exists (ctx, full_path, topic_name, strlen (topic_name), axl_true, axl_true, qos, app_msg, app_msg_size);
	axl_free (full_path);
	
	return result; /* by default report error */
}

/** 
 * @brief Allows to get current queued messages pending to be
 * redelivered on next connection (offline version).
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifier to select the right
 * local session according to its client identifier.
 *
 * @return Amount of messages stored on the provided client_identifier.
 */
int      myqtt_storage_queued_messages_offline (MyQttCtx   * ctx, 
						const char * client_identifier)
{
	char            * full_path;
	DIR             * sub_dir;
	int               count;
	struct dirent   * entry;
#if !defined(_DIRENT_HAVE_D_TYPE)
	char            * aux_path;
#endif

	/* check input values:
	 *
	 * don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0)
		return 0;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", NULL);
	if (! full_path) 
		return 0;

	/* open directory */
	sub_dir = opendir (full_path);
	if (sub_dir == NULL)  {
		axl_free (full_path);
		return 0;
	} /* end if */
	
	/* count files inside messages directory */
	count = 0;
	entry = readdir (sub_dir);
	while (entry) {

#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_REG) == DT_REG) 
			count++;
#else 
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			/* count subscriptions */
			count++;
		}
		axl_free (aux_path);
#endif

		/* get next entry */
		entry = readdir (sub_dir);
	}
	axl_free (full_path);

	closedir (sub_dir);

	return count;
}

/** 
 * @brief Allows to get current queued messages pending to be
 * redelivered on next connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn Connection reference to select the right local session
 * according to its client identifier.
 *
 * @return Number of queued messages associated to the client id
 * running the provided connection or -1 if it fails
 */
int      myqtt_storage_queued_messages         (MyQttCtx   * ctx, 
						MyQttConn  * conn)
{
	if (conn == NULL)
		return -1;

	return myqtt_storage_queued_messages_offline (ctx, conn->client_identifier);
}

/** 
 * @brief Allows to get current storage quota used by queued messages
 * pending to be redelivered on next connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifier to select the right
 * local session according to its client identifier.
 *
 * @return Quota used by the storage selected by the provided client_identifier.
 */
int      myqtt_storage_queued_messages_quota_offline   (MyQttCtx   * ctx, 
							const char * client_identifier)
{
	char            * full_path;
	DIR             * sub_dir;
	int               count;
	struct dirent   * entry;
	int               pos;
#if !defined(_DIRENT_HAVE_D_TYPE)
	char            * aux_path;
#endif

	/* check input values:
	 *
	 * don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0)
		return 0;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", NULL);
	if (! full_path) 
		return 0;

	/* open directory */
	sub_dir = opendir (full_path);
	if (sub_dir == NULL)  {
		axl_free (full_path);
		return 0;
	} /* end if */
	
	/* count files inside messages directory */
	count = 0;
	entry = readdir (sub_dir);
	while (entry) {
#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_REG) == DT_REG)  {
			pos = __myqtt_storage_strpos (ctx, entry->d_name, '-');
			count += __myqtt_storage_get_size_from_file_name (ctx, entry->d_name + pos + 1, NULL);
		}
#else 
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			/* count subscriptions */
			pos = __myqtt_storage_strpos (ctx, aux_path, '-');
			count += __myqtt_storage_get_size_from_file_name (ctx, aux_path + pos + 1, NULL);
		}
		axl_free (aux_path);
#endif

		/* get next entry */
		entry = readdir (sub_dir);
	}
	axl_free (full_path);

	closedir (sub_dir);

	return count;	
}

/** 
 * @brief Allows to get current storage quota used by queued messages
 * pending to be redelivered on next connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn Connection reference to select the right local session
 * according to its client identifier.
 *
 * @return Number of queued messages associated to the client id
 * running the provided connection.
 */
int      myqtt_storage_queued_messages_quota   (MyQttCtx   * ctx, 
						MyQttConn  * conn)
{
	if (conn == NULL)
		return -1;
	return myqtt_storage_queued_messages_quota_offline (ctx, conn->client_identifier);
}

void myqtt_storage_queued_flush_work (MyQttCtx * ctx, MyQttConn * conn)
{
	/* local parameters */
	char            * full_path;
	DIR             * sub_dir;
	int               count;
	struct dirent   * entry;
	char            * aux_path;

	int               qos;
	int               packet_id;
	unsigned char   * msg;
	int               size;

	FILE            * _fcontent;

	/* check input values:
	 *
	 * don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || conn == NULL || conn->client_identifier == NULL || strlen (conn->client_identifier) == 0)
		return;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, conn->client_identifier, "msgs", NULL);
	if (! full_path) 
		return;

	/* open directory */
	sub_dir = opendir (full_path);
	if (sub_dir == NULL)  {
		axl_free (full_path);
		return;
	} /* end if */
	
	/* count files inside messages directory */
	count = 0;
	entry = readdir (sub_dir);
	while (entry) {

		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			/* ok, now parse each message to get */

			/* get packet_id */
			__myqtt_storage_get_values_from_file_name (ctx, entry->d_name, &packet_id, &size, &qos);

			myqtt_log (MYQTT_LEVEL_DEBUG, "Sending offline queued message to conn-id=%d conn=%p packet_id=%d size=%d qos=%d handle=%s",
				   conn->id, conn, packet_id, size, qos, entry->d_name);

			/* open message into memory */
			msg        = axl_new (unsigned char, size + 1);
			_fcontent  = fopen (aux_path, "r");
			if (fread (msg, 1, size, _fcontent) != size) 
				myqtt_log (MYQTT_LEVEL_CRITICAL, "Expected to read %d from file but found different size read, error was: %s",
					   size, myqtt_errno_get_error (errno));
			fclose (_fcontent);

			/* call to resend */
			if (! __myqtt_conn_pub_send_and_handle_reply (ctx, conn, packet_id, qos, aux_path, 60, msg, size)) 
				myqtt_log (MYQTT_LEVEL_CRITICAL, "failed to resend queued message, __myqtt_conn_pub_send_and_handle_reply() failed");
			
			/* nullify to avoid double-free on next call */
			aux_path = NULL;

			/* no need to release msg, or aux_paht
			 * here...this is already done by
			 * __myqtt_conn_pub_send_and_handle_reply() */

			/* count subscriptions */
			count++;
		}
		axl_free (aux_path);

		/* get next entry */
		entry = readdir (sub_dir);
	}
	axl_free (full_path);

	closedir (sub_dir);

	return;
}

axlPointer __myqtt_storage_queued_flush_proxy (axlPointer _conn)
{
	MyQttConn * conn = _conn;
	MyQttCtx  * ctx  = conn->ctx;

	/* call local function */
	myqtt_storage_queued_flush_work (ctx, conn);

	/* signal we have finished flushing */
	conn->flushing = axl_false;

	/* release reference */
	myqtt_conn_unref (conn, "flushing queue");

	return NULL;
}

/** 
 * @brief Allows to redeliver all queued messages associated to the
 * connected session provided.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection that may have pending messages to be redeliver...
 *
 * @return Number of flushed messages, or -1 if it fails.
 */
void      myqtt_storage_queued_flush            (MyQttCtx   * ctx, 
						 MyQttConn  * conn)
{
	if (ctx == NULL || conn == NULL)
		return;

	/* skip if there is another process flushing */
	if (conn->flushing)
		return;

	/* ensure only one process is flushing */
	myqtt_mutex_lock (&conn->op_mutex);
	if (conn->flushing) {
		myqtt_mutex_unlock (&conn->op_mutex);
		return;
	} /* end if */

	if (! myqtt_conn_ref (conn, "flushing queue")) {
		myqtt_mutex_unlock (&conn->op_mutex);
		return;
	} /* end if */

	/* flag we are flushing */
	conn->flushing = axl_true;
	myqtt_mutex_unlock (&conn->op_mutex);

	myqtt_log (MYQTT_LEVEL_DEBUG, "Flushing queued messages for conn-id=%d, conn=%p",
		   conn->id, conn);

	/* call to flush */
	myqtt_thread_pool_new_task (ctx, __myqtt_storage_queued_flush_proxy, conn);

	return;
}

/** 
 * @brief Allows to lock the provided id or fail if it is already in
 * use.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifer where to lock the provided pkgid.
 *
 * @param pkg_id The packet id to lock. 
 *
 * @return axl_true if the pkgid was locked, otherwise axl_false is returned. 
 */
axl_bool myqtt_storage_lock_pkgid_offline (MyQttCtx      * ctx, 
					   const char    * client_identifier,
					   int             pkg_id)
{
	int    handle;
	char * full_path;
	char * ref;

	/* don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0 || pkg_id < 1)
		return axl_false;

	/* create full path to lock pkgid */
	ref       = axl_strdup_printf ("%d", pkg_id);
	if (ref == NULL)
		return axl_false;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", ref, NULL);
	axl_free (ref);
	if (full_path == NULL) 
		return axl_false;

	/* open directory handle */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Attempting to lock pkgid file at: %s", full_path);
	handle = open (full_path, O_CREAT | O_EXCL, 0600);
	axl_free (full_path);
	if (handle < 0)
		return axl_false;
	close (handle);

	return axl_true;	
}

/** 
 * @brief Allows to lock the provided id or fail if it is already in
 * use.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection where the operation takes place, using its session.
 *
 * @param pkg_id The packet id to lock. 
 *
 * @return axl_true if the pkgid was locked, otherwise axl_false is returned. 
 */
axl_bool myqtt_storage_lock_pkgid (MyQttCtx * ctx, MyQttConn * conn, int pkg_id)
{
	/* avoid segfault when conn reference is NULL */
	if (conn == NULL)
		return axl_false;

	return myqtt_storage_lock_pkgid_offline (ctx, conn->client_identifier, pkg_id);
}

/** 
 * @brief Allows to release the provided id from the current session
 * associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier Client identifier for which the release operation will be applied
 *
 * @param pkg_id The packet id to release. 
 *
 * @return axl_true if the pkgid was releaseed, otherwise axl_false is returned. 
 */
void     myqtt_storage_release_pkgid_offline    (MyQttCtx      * ctx, 
						 const char    * client_identifier,
						 int             pkg_id)
{
	char * full_path;
	char * ref;

	/* don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0 || pkg_id < 1)
		return;

	/* create full path to lock pkgid */
	ref       = axl_strdup_printf ("%d", pkg_id);
	if (ref == NULL)
		return;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", ref, NULL);
	axl_free (ref);
	if (full_path == NULL) 
		return;

	/* open directory handle */
	unlink (full_path);
	axl_free (full_path);

	return;

}

/** 
 * @brief Allows to release the provided id from the current session
 * associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection where the operation takes place, using its session.
 *
 * @param pkg_id The packet id to release. 
 *
 * @return axl_true if the pkgid was releaseed, otherwise axl_false is returned. 
 */
void     myqtt_storage_release_pkgid (MyQttCtx * ctx, MyQttConn * conn, int pkg_id)
{
	/* avoid segfault when conn reference is NULL */
	if (conn == NULL)
		return;
	return myqtt_storage_release_pkgid_offline (ctx, conn->client_identifier, pkg_id);
}

/** 
 * @brief Allows to recover session stored by the server with current
 * storage for the provided connection.
 *
 * @param ctx The context where the operation will take place.
 *
 * @param conn The connection that is requested to recover session. 
 *
 * @return axl_true if session was recovered, otherwise axl_false is
 * returned.
 */
axl_bool myqtt_storage_session_recover (MyQttCtx * ctx, MyQttConn * conn)
{
	/* call to iterate and count */
	return __myqtt_storage_iteration (ctx, conn->client_identifier, conn, /* register */ axl_true, /* offline */ axl_false);
}

/** 
 * @internal Function that allows to recover client identifiers and
 * subscriptions from local storage. This function is only useful for
 * server side and it is not meant to be used directly by API
 * consumers.
 *
 * @return Return the number of subscription that were loaded.
 */
int     myqtt_storage_load             (MyQttCtx      * ctx)
{
	DIR           * sub_dir;
	struct dirent * entry;
	int             entries = 0;

	if (ctx == NULL || ! ctx->storage_path) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to load local storage because context (%p) is not defined or storage path is empty: %s",
			   ctx, ctx->storage_path ? ctx->storage_path : "<not defined>");
		return 0;
	} /* end if */

	/* skip local storage loading if already done previously */
	if (ctx->local_storage)
		return 0;

	myqtt_mutex_lock (&ctx->ref_mutex);
	if (ctx->local_storage) {
		myqtt_mutex_unlock (&ctx->ref_mutex);
		return 0;
	} /* end if */

	/* init server side hashs to handle subscriptions */
	/*** on-line subscriptions ***/
	ctx->subs              = axl_hash_new (axl_hash_string, axl_hash_equal_string);
	ctx->wild_subs         = axl_hash_new (axl_hash_string, axl_hash_equal_string);

	/*** off-line subscriptiosn ***/
	ctx->offline_subs      = axl_hash_new (axl_hash_string, axl_hash_equal_string);
	ctx->offline_wild_subs = axl_hash_new (axl_hash_string, axl_hash_equal_string);

	/* now find all local identifiers that have at least one
	 * subscription */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Loading storage from: %s", ctx->storage_path ? ctx->storage_path : "<null>");
	sub_dir = opendir (ctx->storage_path);
	if (sub_dir == NULL)
		goto finish;

	/* get next entry */
	entry = readdir (sub_dir);
	while (entry) {
		/* skip default directories */
		if (axl_cmp (entry->d_name, ".") || axl_cmp (entry->d_name, ".."))
			goto next_entry;

#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_DIR) != DT_DIR) 
			goto next_entry;
#else
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_DIR)) {
			axl_free (aux_path);
			goto next_entry;
		} /* end if */
		axl_free (aux_path);
#endif

		/* found directory (a session identifier) */
		if (myqtt_storage_sub_count_offline (ctx, entry->d_name) > 0)  {
			/* found entry with subscriptions */
			myqtt_log (MYQTT_LEVEL_DEBUG, "Checking subscriptions for %s (%d)", entry->d_name, myqtt_storage_sub_count_offline (ctx, entry->d_name)); 
			entries += __myqtt_storage_iteration (ctx, entry->d_name, NULL, /* register */ axl_true, /* offline */ axl_true);
		} /* end if */

	next_entry:
		/* next entry */
		entry = readdir (sub_dir);
	} /* end while */

finish:
	/* notify it is already loaded */
	ctx->local_storage = axl_true;
	myqtt_mutex_unlock (&ctx->ref_mutex);
	closedir (sub_dir);

	return entries;
}


/** 
 * @brief Allows to configure the storage location for the provided
 * MyQtt context.
 *
 * @param ctx The context where the operation takes place. It cannot be NULL.
 *
 * @param storage_path The storage dir to configure. It cannot be NULL
 * or empty.
 *
 * @param hash_size Hashing size for the storage path used. This value
 * will be used to split internal storage in the given values.
 * Recomended value is 4096.
 *
 *
 * @return axl_true if the path was correctly set, otherwise axl_false is returned.
 */
axl_bool     myqtt_storage_set_path (MyQttCtx * ctx, const char * storage_path, int hash_size)
{
	if (ctx == NULL || storage_path == NULL || strlen (storage_path) == 0) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to configure storage path, context is NULL(%d), or storage path is not defined (%s)",
			   ctx, (ctx && ctx->storage_path) ? ctx->storage_path : "<undefined>");
		return axl_false;
	}

	/* lock during operation */
	myqtt_mutex_lock (&ctx->ref_mutex);

	/* update storage path */
	if (ctx->storage_path)
		axl_free (ctx->storage_path);
	ctx->storage_path = axl_strdup (storage_path);

	/* update hash size */
	ctx->storage_path_hash_size = hash_size;

	/* now unlock */
	myqtt_mutex_unlock (&ctx->ref_mutex);

	return axl_true;
}

void __myqtt_storage_get_retained_topics_dir (MyQttCtx * ctx, const char * topic_filter, const char * full_path, axlList * list)
{
	unsigned char * topic_name = NULL;
	int             ref_size   = 0;
	DIR           * sub_dir;
	struct dirent * entry;
	char          * aux_path;
	axl_bool        added_to_the_list;

	/* try to open path */
	sub_dir = opendir (full_path);
	if (sub_dir == NULL) {
		__myqtt_storage_error_report (ctx, "Unable to open %s", full_path);
		return;
	} /* end if */

	entry   = readdir (sub_dir);
	while (sub_dir && entry) {

		/* get next entry and skip those we are not interested in */
		if (axl_cmp (".", entry->d_name) || axl_cmp ("..", entry->d_name)) {
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* create full path and do some checks */
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (! myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			entry = readdir (sub_dir);
			axl_free (aux_path);
			continue;
		} /* end if */

		/* skip those files that includes .msg (which is the content of the message retained) */
		if (strstr (aux_path, ".msg")) {
			entry = readdir (sub_dir);
			axl_free (aux_path);
			continue;
		}
		
		/* skip known extensions that aren't valid */
		if (strstr (aux_path, "~")) {
			entry = readdir (sub_dir);
			axl_free (aux_path);
			continue;
		}

		/* call to load the content from the file */
		topic_name = NULL;
		__myqtt_storage_read_content_into_reference (ctx, aux_path, &topic_name, &ref_size);
		axl_free (aux_path);
		added_to_the_list = axl_false;
		
		/* save into the list the topic name recovered */
		if (topic_name && strlen ((const char *) topic_name) > 0) {
			/* check if the topic matches with the filter */
			if (myqtt_reader_topic_filter_match ((const char *) topic_name, topic_filter)) {
				axl_list_append (list, topic_name);
				/* notify added to the list */
				added_to_the_list = axl_true;
			}
		} /* end if */

		if (! added_to_the_list) {
			/* release if it wasn't added to the list */
			axl_free (topic_name);
		} /* end if */

		/* get next entry */
		entry   = readdir (sub_dir);
	} /* end if */

	closedir (sub_dir);

	return;
}

/** 
 * @brief Allows to get the list of topics with message retention
 * stored, filtered by the provided topic_filter
 *
 * @param ctx The context where the operation takes place
 *
 * @param topic_filter The filter to allow selecting those topics that
 * matches and have a message pending due to retain flag configured in
 * the last publish
 *
 * @return A list of retained topics or NULL if it fails. The list
 * returned may be empty. Use axl_list_free (to release result
 * reported).
 */
axlList * myqtt_storage_get_retained_topics (MyQttCtx * ctx, const char * topic_filter)
{

	char          * full_path;
	char          * aux_path;
	DIR           * sub_dir;
	struct dirent * entry;
	axlList       * list;

	/* get full path to subscriptions */
	full_path = myqtt_support_build_filename (ctx->storage_path, "retained", NULL);
	if (full_path == NULL) 
		return NULL; /* allocation failure */

	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		axl_free (full_path);
		return NULL; /* directory do not exists */
	} /* end if */

	/* try to open path */
	sub_dir = opendir (full_path);
	if (sub_dir == NULL) {
		__myqtt_storage_error_report (ctx, "Unable to open %s", full_path);
		axl_free (full_path);
		return NULL;
	} /* end if */

	/* create list */
	list = axl_list_new (axl_list_always_return_1, axl_free);

	entry   = readdir (sub_dir);
	while (sub_dir && entry) {

		/* get next entry and skip those we are not interested in */
		if (axl_cmp (".", entry->d_name) || axl_cmp ("..", entry->d_name)) {
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* check if it is a directory */
#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_DIR) == DT_DIR) {
			/* count subscriptions */
			aux_path  = myqtt_support_build_filename (full_path, entry->d_name, NULL);
			__myqtt_storage_get_retained_topics_dir (ctx, topic_filter, aux_path, list);
			axl_free (aux_path);
		}
#else 
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_DIR)) {
			/* load into list topic filters found */
			__myqtt_storage_get_retained_topics_dir (ctx, topic_filter, aux_path, list);
		}
		axl_free (aux_path);
#endif

		/* get next entry */
		entry   = readdir (sub_dir);
	}

	closedir (sub_dir);
	axl_free (full_path);

	return list;
}
       
/** 
 * @} 
 */
