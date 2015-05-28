/** 
 *  PyMyQtt: MyQtt Library Python bindings
 *  Copyright (C) 2013 Advanced Software Production Line, S.L.
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
 *         info@aspl.es - http://www.aspl.es/myqtt
 */

#include <py_myqtt_conn.h>
#include <myqtt-conn-private.h>

struct _PyMyQttConn {
	/* header required to initialize python required bits for
	   every python object */
	PyObject_HEAD

	/* pointer to the MyQttConn object */
	MyQttConn * conn;

	/** 
	 * @internal variable used to signal the type to close the
	 * conn wrapped (MyQttConn) when the reference
	 * PyMyQttConn is garbage collected.
	 */
	axl_bool           close_ref;

	/** 
	 * @brief Allows to skip the automatic conn close when
	 * python object is collected.
	 */
	axl_bool         skip_conn_close;
};

#define PY_MYQTT_CONN_CHECK_NOT_ROLE(py_conn, role, method)                                                \
do {                                                                                                              \
	if (myqtt_conn_get_role (((PyMyQttConn *)py_conn)->conn) == role) {                         \
	         py_myqtt_log (PY_MYQTT_CRITICAL,                                                               \
                                "trying to run a method %s not supported by the role %d, conn id: %d",      \
				method, role, myqtt_conn_get_id (((PyMyQttConn *)py_conn)->conn));  \
	         Py_INCREF(Py_None);                                                                              \
		 return Py_None;                                                                                  \
	}                                                                                                         \
} while(0);

/** 
 * @internal function that maps conn roles to string values.
 */
const char * __py_myqtt_conn_stringify_role (MyQttConn * conn)
{
	/* check known roles to return its appropriate string */
	switch (myqtt_conn_get_role (conn)) {
	case MyQttRoleInitiator:
		return "initiator";
	case MyQttRoleListener:
		return "listener";
	case MyQttRoleMasterListener:
		return "master-listener";
	default:
		break;
	}

	/* return unknown string */
	return "unknown";
}
		 

/** 
 * @brief Allows to get the MyQttConn reference inside the
 * PyMyQttConn.
 *
 * @param py_conn The reference that holds the conn inside.
 *
 * @return A reference to the MyQttConn inside or NULL if it fails.
 */
MyQttConn * py_myqtt_conn_get  (PyObject * py_conn)
{
	PyMyQttConn * _py_conn = (PyMyQttConn *) py_conn;

	/* return NULL reference */
	if (_py_conn == NULL)
		return NULL;
	/* return py conn */
	return _py_conn->conn;
}

static int py_myqtt_conn_init_type (PyMyQttConn *self, PyObject *args, PyObject *kwds)
{
    return 0;
}

/** 
 * @brief Function used to allocate memory required by the object myqtt.Conn
 */
static PyObject * py_myqtt_conn_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyMyQttConn        * self;
	const char         * client_identifier = NULL;
	axl_bool             clean_session     = axl_false;
	int                  keep_alive        = 0;

	const char         * host       = NULL;
	const char         * port       = NULL;
	PyObject           * py_myqtt_ctx = NULL;

	/* now parse arguments */
	static char *kwlist[] = {"ctx", "host", "port", "client_identifier", "clean_session", "keep_alive", NULL};

	/* create the object */
	self = (PyMyQttConn *)type->tp_alloc(type, 0);

	/* check args */
	if (args != NULL) {
		/* parse and check result */
		if (! PyArg_ParseTupleAndKeywords(args, kwds, "|Osszii", kwlist, 
						  &py_myqtt_ctx, &host, &port, &client_identifier, &clean_session, &keep_alive)) 
			return NULL;

		/* check for empty creation */
		if (py_myqtt_ctx == NULL) {
			py_myqtt_log (PY_MYQTT_DEBUG, "found empty request to create a PyMyQttConn ref..");
			return (PyObject *) self;
		}

		/* check that py_myqtt_ctx is indeed a myqtt ctx */
		if (! py_myqtt_ctx_check (py_myqtt_ctx)) {
			PyErr_Format (PyExc_ValueError, "Expected to receive a myqtt.Ctx object but received something different");
			return NULL;
		}

		/* set default timeout */
		myqtt_conn_connect_timeout (py_myqtt_ctx_get (py_myqtt_ctx), 60000000);

		/* allow threads */
		Py_BEGIN_ALLOW_THREADS

		/* create the myqtt conn in a blocking manner */
		self->conn = myqtt_conn_new (py_myqtt_ctx_get (py_myqtt_ctx),
					     client_identifier, clean_session, keep_alive,
					     host, port,
					     NULL,
					     NULL, NULL);

		/* end threads */
		Py_END_ALLOW_THREADS

		/* signal this instance as a master copy to be closed
		 * if the reference is collected and the conn is
		 * working */
		self->close_ref = axl_true;

		if (myqtt_conn_is_ok (self->conn, axl_false)) {
			py_myqtt_log (PY_MYQTT_DEBUG, "created conn id %d, with %s:%s (self: %p, conn: %p)",
				       myqtt_conn_get_id (self->conn), 
				       myqtt_conn_get_host (self->conn),
				       myqtt_conn_get_port (self->conn),
				       self, self->conn);
		} else {
			py_myqtt_log (PY_MYQTT_CRITICAL, "failed to connect with %s:%s, conn id: %d",
				       myqtt_conn_get_host (self->conn),
				       myqtt_conn_get_port (self->conn),
				       myqtt_conn_get_id (self->conn));
		} /* end if */
	} /* end if */

	return (PyObject *)self;
}

/** 
 * @brief Function used to finish and dealloc memory used by the object myqtt.Conn
 */
static void py_myqtt_conn_dealloc (PyMyQttConn* self)
{
#if defined(ENABLE_PY_MYQTT_LOG)
	int conn_id = myqtt_conn_get_id (self->conn);
#endif
	int ref_count;

	py_myqtt_log (PY_MYQTT_DEBUG, "finishing PyMyQttConn id: %d (%p, MyQttConn %p, role: %s, close-ref: %d)", 
		       conn_id, self, self->conn, __py_myqtt_conn_stringify_role (self->conn), self->close_ref);

	/* finish the conn in the case it is no longer referenced */
	if (myqtt_conn_is_ok (self->conn, axl_false) && self->close_ref) {
		py_myqtt_log (PY_MYQTT_DEBUG, "shutting down MQTT session associated at conn finalize id: %d (conn is ok, and close_ref is activated, refs: %d)", 
			       myqtt_conn_get_id (self->conn),
			       myqtt_conn_ref_count (self->conn));

		/* shutdown conn if itsn't flagged that way */
		if (! self->skip_conn_close) {
			/* allow threads */
			Py_BEGIN_ALLOW_THREADS
			myqtt_conn_shutdown (self->conn);
			/* end threads */
			Py_END_ALLOW_THREADS
		}

		ref_count = myqtt_conn_ref_count (self->conn);
		myqtt_conn_unref (self->conn, "py_myqtt_conn_dealloc when is ok");
		py_myqtt_log (PY_MYQTT_DEBUG, "ref count after close: %d", ref_count - 1);
	} else {
		py_myqtt_log (PY_MYQTT_DEBUG, "unref the conn id: %d", myqtt_conn_get_id (self->conn));
		/* only unref the conn */
		myqtt_conn_unref (self->conn, "py_myqtt_conn_dealloc");
	} /* end if */

	/* nullify */
	self->conn = NULL;

	/* free the node it self */
	self->ob_type->tp_free ((PyObject*)self);

	py_myqtt_log (PY_MYQTT_DEBUG, "terminated PyMyQttConn dealloc with id: %d (self: %p)", conn_id, self);

	return;
}

/** 
 * @brief Direct wrapper for the myqtt_conn_is_ok function. 
 */
static PyObject * py_myqtt_conn_is_ok (PyMyQttConn* self)
{
	PyObject *_result;

	/* call to check conn and build the value with the
	   result. Do not free the conn in the case of
	   failure. */
	_result = Py_BuildValue ("i", myqtt_conn_is_ok (self->conn, axl_false));
	
	return _result;
}

/** 
 * @brief Direct wrapper for the myqtt_conn_close function. 
 */
static PyObject * py_myqtt_conn_close (PyMyQttConn* self)
{
	PyObject       * _result;
	axl_bool         result;
	MyQttPeerRole   role     = MyQttRoleUnknown;
	const char     * str_role = NULL;

	if (self->conn) {
		py_myqtt_log (PY_MYQTT_DEBUG, "closing conn id: %d (%s, refs: %d)",
			       myqtt_conn_get_id (self->conn), 
			       __py_myqtt_conn_stringify_role (self->conn), 
			       myqtt_conn_ref_count (self->conn));
		/* get peer role to avoid race conditions */
		role     = myqtt_conn_get_role (self->conn);
		str_role = __py_myqtt_conn_stringify_role (self->conn);
	} /* end if */

	/* according to the conn role and status, do a shutdown
	 * or a close */
	if (role == MyQttRoleMasterListener &&
	    (myqtt_conn_is_ok (self->conn, axl_false))) {
		py_myqtt_log (PY_MYQTT_DEBUG, "shutting down working master listener conn id=%d", 
			       myqtt_conn_get_id (self->conn));
		result = axl_true;
		/* allow threads */
		Py_BEGIN_ALLOW_THREADS
		myqtt_conn_shutdown (self->conn);
		/* end threads */
		Py_END_ALLOW_THREADS
	} else  {
		py_myqtt_log (PY_MYQTT_DEBUG, "closing conn id=%d (role: %s)", 
			       myqtt_conn_get_id (self->conn), str_role);
		/* allow threads */
		Py_BEGIN_ALLOW_THREADS
		result  = myqtt_conn_close (self->conn);
		/* end threads */
		Py_END_ALLOW_THREADS
	} /* end if */
	_result = Py_BuildValue ("i", result);

	/* check to nullify conn reference in the case the conn is closed */
	if (result) {
		/* check if we have to unref the conn in the
		 * case of a master listener */
		if (role == MyQttRoleMasterListener)
			myqtt_conn_unref (self->conn, "py_myqtt_conn_close (master-listener)");

		py_myqtt_log (PY_MYQTT_DEBUG, "close ok, nullifying..");
		self->conn = NULL; 
	}

	return _result;
}

/** 
 * @brief Direct wrapper for the myqtt_conn_shutdown function. 
 */
PyObject * py_myqtt_conn_shutdown (PyMyQttConn* self)
{
	py_myqtt_log (PY_MYQTT_DEBUG, "calling to shutdown conn id: %d, self: %p",
		       myqtt_conn_get_id (self->conn), self);

	/* allow threads */
	Py_BEGIN_ALLOW_THREADS
	/* shut down the conn */
	myqtt_conn_shutdown (self->conn);
	/* end threads */
	Py_END_ALLOW_THREADS

	/* return none */
	Py_INCREF (Py_None);
	return Py_None;
}

/** 
 * @brief Increment reference counting
 */
static PyObject * py_myqtt_conn_incref (PyMyQttConn* self)
{
	/* close the conn */
	Py_INCREF (__PY_OBJECT (self));
	Py_INCREF (Py_None);
	return Py_None;
}

/** 
 * @brief Decrement reference counting.
 */
static PyObject * py_myqtt_conn_decref (PyMyQttConn* self)
{
	/* close the conn */
	Py_DECREF (__PY_OBJECT (self));
	Py_INCREF (Py_None);
	return Py_None;
}

/** 
 * @brief Allows to nullify the internal reference to the
 * MyQttConn object.
 */ 
void                 py_myqtt_conn_nullify  (PyObject           * py_conn)
{
	PyMyQttConn * _py_conn = (PyMyQttConn *) py_conn;
	if (py_conn == NULL)
		return;
	
	/* nullify the conn to make it available to other owner
	 * or process */
	_py_conn->conn = NULL;
	return;
}

/** 
 * @brief Direct wrapper for the myqtt_conn_status function. 
 */
PyObject * py_myqtt_conn_last_err (PyMyQttConn* self)
{
	PyObject *_result;

	/* call to check conn and build the value with the
	   result. Do not free the conn in the case of
	   failure. */
	_result = Py_BuildValue ("i", myqtt_conn_get_last_err (self->conn));
	
	return _result;
}

/** 
 * @brief This function implements the generic attribute getting that
 * allows to perform complex member resolution (not merely direct
 * member access).
 */
PyObject * py_myqtt_conn_get_attr (PyObject *o, PyObject *attr_name) {
	const char         * attr = NULL;
	PyObject           * result;
	PyMyQttConn * self = (PyMyQttConn *) o;

	/* now implement other attributes */
	if (! PyArg_Parse (attr_name, "s", &attr))
		return NULL;

	/* printf ("received request to return attribute value of '%s'..\n", attr); */

	if (axl_cmp (attr, "error_msg")) {
		/* found error_msg attribute */
		return Py_BuildValue ("z", myqtt_conn_get_code_to_err (myqtt_conn_get_last_err (self->conn)));
	} else if (axl_cmp (attr, "status")) {
		/* found status attribute */
		return Py_BuildValue ("i", myqtt_conn_get_last_err (self->conn));
	} else if (axl_cmp (attr, "server_name")) {
		/* found server_name */
		return Py_BuildValue ("z", myqtt_conn_get_server_name (self->conn));
	} else if (axl_cmp (attr, "host")) {
		/* found host attribute */
		return Py_BuildValue ("z", myqtt_conn_get_host (self->conn));
	} else if (axl_cmp (attr, "host_ip")) {
		/* found host attribute */
		return Py_BuildValue ("z", myqtt_conn_get_host_ip (self->conn));
	} else if (axl_cmp (attr, "port")) {
		/* found port attribute */
		return Py_BuildValue ("z", myqtt_conn_get_port (self->conn));
	} else if (axl_cmp (attr, "local_addr")) {
		/* found local_addr attribute */
		return Py_BuildValue ("z", myqtt_conn_get_local_addr (self->conn));
	} else if (axl_cmp (attr, "local_port")) {
		/* found local_port attribute */
		return Py_BuildValue ("z", myqtt_conn_get_local_port (self->conn));
	} else if (axl_cmp (attr, "role")) {
		/* found role attribute */
		switch (myqtt_conn_get_role (self->conn)) {
		case MyQttRoleInitiator:
			return Py_BuildValue ("s", "initiator");
		case MyQttRoleListener:
			return Py_BuildValue ("s", "listener");
		case MyQttRoleMasterListener:
			return Py_BuildValue ("s", "master-listener");
		default:
			break;
		}
		return Py_BuildValue ("s", "unknown");
	} else if (axl_cmp (attr, "ctx")) {
		/* found ctx attribute */
		return py_myqtt_ctx_create (CONN_CTX (self->conn));
	} else if (axl_cmp (attr, "id")) {
		/* return integer value */
		return Py_BuildValue ("i", myqtt_conn_get_id (self->conn));
	} else if (axl_cmp (attr, "ref_count")) {
		/* return integer value */
		return Py_BuildValue ("i", myqtt_conn_ref_count (self->conn));
	} /* end if */

	/* printf ("Attribute not found: '%s'..\n", attr); */

	/* first implement generic attr already defined */
	result = PyObject_GenericGetAttr (o, attr_name);
	if (result)
		return result;
	
	return NULL;
}

/** 
 * @internal The following is an auxiliar structure used to bridge
 * set_on_close_full call into python. Because MyQtt version allows
 * configuring several handlers at the same time, it is required to
 * track a different object with full state to properly bridge the
 * notification received into python without mixing notifications. See
 * py_myqtt_conn_set_on_close to know how this is used.
 */
typedef struct _PyMyQttConnSetOnCloseData {
	PyObject           * py_conn;
	PyObject           * on_close;
	PyObject           * on_close_data;
} PyMyQttConnSetOnCloseData;

void py_myqtt_conn_set_on_close_handler (MyQttConn * conn, 
						axlPointer         _on_close_obj)
{
	PyMyQttConnSetOnCloseData * on_close_obj = _on_close_obj;
	PyGILState_STATE                   state;
	PyObject                         * args;
	PyObject                         * result;
	MyQttCtx                        * ctx = CONN_CTX(conn);

	/* notify on close notification received */
	py_myqtt_log (PY_MYQTT_DEBUG, "found on close notification for conn id=%d, (internal: %p)", 
		       myqtt_conn_get_id (conn), _on_close_obj);
	
	/*** bridge into python ***/
	/* acquire the GIL */
	state = PyGILState_Ensure();

	/* create a tuple to contain arguments */
	args = PyTuple_New (2);

	Py_INCREF (on_close_obj->py_conn);
	PyTuple_SetItem (args, 0, __PY_OBJECT (on_close_obj->py_conn));
	Py_INCREF (on_close_obj->on_close_data);
	PyTuple_SetItem (args, 1, on_close_obj->on_close_data);

	/* record handler */
	START_HANDLER (on_close_obj->on_close);

	/* now invoke */
	result = PyObject_Call (on_close_obj->on_close, args, NULL);

	/* unrecord handler */
	CLOSE_HANDLER (on_close_obj->on_close);

	py_myqtt_log (PY_MYQTT_DEBUG, "conn on close notification finished, checking for exceptions..");
	py_myqtt_handle_and_clear_exception (__PY_OBJECT (on_close_obj->py_conn));

	Py_XDECREF (result);
	Py_DECREF (args);

	/* now release the rest of data */
	Py_DECREF (on_close_obj->py_conn);
	Py_DECREF (on_close_obj->on_close);
	Py_DECREF (on_close_obj->on_close_data);

	/* unref the node itself */
	axl_free  (on_close_obj);

	/* release the GIL */
	PyGILState_Release(state);

	return;
}

PyObject * py_myqtt_conn_sub (PyObject * self, PyObject * args, PyObject * kwds)
{
	int          wait_sub  = 10;
	const char * topic     = NULL;
	int          qos       = 0;
	axl_bool     result;
	int          sub_result = 0;
	MyQttConn  * conn;
		
	
	/* now parse arguments */
	static char *kwlist[] = {"topic", "qos", "wait_sub", NULL};

	/* parse and check result */
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s|ii", kwlist, &topic, &qos, &wait_sub))
		return NULL;

	/* allow threads */
	Py_BEGIN_ALLOW_THREADS

	/* call to subscribe */
	conn   = py_myqtt_conn_get (self);
	result = myqtt_conn_sub (conn, wait_sub, topic, qos, &sub_result);

	/* end threads */
	Py_END_ALLOW_THREADS

	if (! result) {
		/* return (status, None) */
		Py_INCREF (Py_None);
		return Py_BuildValue ("(iO)", result, Py_None);
	} /* end if */

	/* return Qos reported (status, qos)  */
	return Py_BuildValue ("(ii)", result, sub_result);
}


PyObject * py_myqtt_conn_set_on_close (PyObject * self, PyObject * args, PyObject * kwds)
{
	PyObject                  * on_close      = NULL;
	PyObject                  * on_close_data = Py_None;
	PyMyQttConnSetOnCloseData * on_close_obj;
	axl_bool                    insert_last   = axl_true;
	
	/* now parse arguments */
	static char *kwlist[] = {"on_close", "on_close_data", "insert_last", NULL};

	/* parse and check result */
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|Oi", kwlist, &on_close, &on_close_data, &insert_last)) 
		return NULL;

	/* check handler received */
	if (on_close == NULL || ! PyCallable_Check (on_close)) {
		py_myqtt_log (PY_MYQTT_CRITICAL, "received on_close handler which is not a callable object");
		return NULL;
	} /* end if */

	/* configure an on close handler to bridge into python */
	on_close_obj = axl_new (PyMyQttConnSetOnCloseData, 1);

	/* acquire a reference to the conn */
	on_close_obj->py_conn = self;
	Py_INCREF (self);

	/* configure on_close handler */
	on_close_obj->on_close = on_close;
	Py_INCREF (on_close);

	/* configure on_close_data handler data */
	if (on_close_data == NULL)
		on_close_data = Py_None;
	on_close_obj->on_close_data = on_close_data;
	Py_INCREF (on_close_data);

	/* configure on_close_full */
	myqtt_conn_set_on_close (
               /* the conn with on close */
	       py_myqtt_conn_get (on_close_obj->py_conn),
	       /* insert last */
	       insert_last,
	       /* the handler */
	       py_myqtt_conn_set_on_close_handler, 
	       /* the object with all references */
	       on_close_obj);

	/* create a handle that allows to remove this particular
	   handler. This handler can be used to remove the on close
	   handler */
	return py_myqtt_handle_create (on_close_obj, NULL);
}

static PyObject * py_myqtt_conn_remove_on_close (PyObject * self, PyObject * args, PyObject * kwds)
{
	PyObject                         * handle      = NULL;
	PyMyQttConnSetOnCloseData * on_close_obj;
	axl_bool                           result;
	
	/* now parse arguments */
	static char *kwlist[] = {"handle", NULL};

	/* parse and check result */
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &handle)) 
		return NULL;

	/* if None is received, just return */
	if (! py_myqtt_handle_check (handle)) {
		Py_INCREF (Py_False);
		return Py_False;
	} /* end if */
	
	/* get on_close object */
	on_close_obj = py_myqtt_handle_get (handle);
	py_myqtt_handle_nullify (handle);

	/* call to remove close handler */
	result = myqtt_conn_remove_on_close (py_myqtt_conn_get (self), 
					     py_myqtt_conn_set_on_close_handler,
					     on_close_obj);

	/* finish close object */
	if (on_close_obj) {
		py_myqtt_log (PY_MYQTT_DEBUG, "finishing on close object reference (%p)", on_close_obj);
		Py_DECREF (on_close_obj->py_conn);
		Py_DECREF (on_close_obj->on_close);
		Py_DECREF (on_close_obj->on_close_data);
		axl_free (on_close_obj);
	} /* end if */

	if (result) {
		/* handler removed */
		Py_INCREF (Py_True);
		return Py_True;
	} /* end if */

	Py_INCREF (Py_False);
	return Py_False;
}

static PyObject * py_myqtt_conn_set_data (PyMyQttConn * self, PyObject * args)
{
	const char  * key = NULL; 
	PyObject    * obj = NULL;

	/* parse and check result */
	if (! PyArg_ParseTuple (args, "sO", &key, &obj)) 
		return NULL;
	
	/* check key to not be NULL or empty */
	if (key == NULL || strlen (key) == 0) {
		PyErr_Format (PyExc_ValueError, "Key data index is NULL or empty, this is not allowed.");
		return NULL;
	} /* end if */
	
	/* increment object ref count */
	Py_INCREF (obj);

	/* store in the conn */
	myqtt_conn_set_data_full (self->conn, axl_strdup (key), obj, axl_free, (axlDestroyFunc) py_myqtt_decref);

	/* done, return ok */
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject * py_myqtt_conn_skip_conn_close (PyMyQttConn * self, PyObject * args)
{
	axl_bool skip_conn_close = axl_true;

	/* parse and check result */
	if (! PyArg_ParseTuple (args, "|i", &skip_conn_close))
		return NULL;

	/* set value received */
	self->skip_conn_close = skip_conn_close;
	
	/* done, return ok */
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject * py_myqtt_conn_block (PyMyQttConn * self, PyObject * args)
{
	axl_bool block = axl_true;

	/* parse and check result */
	if (! PyArg_ParseTuple (args, "|i", &block))
		return NULL;

	/* call to block conn */
	myqtt_conn_block (self->conn, block);

	/* done, return ok */
	Py_INCREF (Py_None);
	return Py_None;
}

static PyObject * py_myqtt_conn_is_blocked (PyMyQttConn * self, PyObject * args)
{
	axl_bool is_blocked;

	/* call to block conn */
	is_blocked = myqtt_conn_is_blocked (self->conn);

	/* done, return ok */
	if (is_blocked) {
		Py_INCREF (Py_True);
		return Py_True;
	}
	Py_INCREF (Py_False);
	return Py_False;
}

static PyObject * py_myqtt_conn_get_data (PyMyQttConn * self, PyObject * args)
{
	const char  * key = NULL; 
	PyObject    * obj = NULL;

	/* parse and check result */
	if (! PyArg_ParseTuple (args, "s", &key)) 
		return NULL;
	
	/* check key to not be NULL or empty */
	if (key == NULL || strlen (key) == 0) {
		PyErr_Format (PyExc_ValueError, "Key data index is NULL or empty, this is not allowed.");
		return NULL;
	} /* end if */
	
	/* get the reference */
	obj = myqtt_conn_get_data (self->conn, key);
	if (obj == NULL)
		obj = Py_None;

	/* return the object reference */
	Py_INCREF (obj);
	return obj;
}

static PyMethodDef py_myqtt_conn_methods[] = { 
	/* is_ok */
	{"is_ok", (PyCFunction) py_myqtt_conn_is_ok, METH_NOARGS,
	 "Allows to check current myqtt.Conn status. In the case False is returned the conn is no longer operative. "},
	/* sub */
	{"sub", (PyCFunction) py_myqtt_conn_sub, METH_VARARGS | METH_KEYWORDS,
	 "API wrapper for myqtt_conn_sub. This method allows to subscribe to a particular topic."},
	/* set_on_close */
	{"set_on_close", (PyCFunction) py_myqtt_conn_set_on_close, METH_VARARGS | METH_KEYWORDS,
	 "API wrapper for myqtt_conn_set_on_close_full. This method allows to configure a handler which will be called in case the conn is closed. This is useful to detect client or server broken conn."},
	/* remove_on_close */
	{"remove_on_close", (PyCFunction) py_myqtt_conn_remove_on_close, METH_VARARGS | METH_KEYWORDS,
	 "API wrapper for myqtt_conn_remove_on_close_full. This method allows to remove a particular on close handler installed by the method .set_on_close."},
	/* close */
	{"close", (PyCFunction) py_myqtt_conn_close, METH_NOARGS,
	 "Allows to close a the MQTT session (myqtt.Conn) following all MQTT close negotation phase. The method returns True in the case the conn was cleanly closed, otherwise False is returned. If this operation finishes properly, the reference should not be used."},
	/* shutdown */
	{"shutdown", (PyCFunction) py_myqtt_conn_shutdown, METH_NOARGS,
	 "Allows to shutdown the MQTT session. This operation closes the underlaying transport without going into the full MQTT close process. It is still required to call to .close method to fully finish the conn. After the shutdown the caller can still use the reference and check its status. After a close operation the conn cannot be used again."},
	/* incref */
	{"incref", (PyCFunction) py_myqtt_conn_incref, METH_NOARGS,
	 "Allows to increment reference counting of the python object (myqtt.Conn) holding the conn."},
	/* decref */
	{"decref", (PyCFunction) py_myqtt_conn_decref, METH_NOARGS,
	 "Allows to decrement reference counting of the python object (myqtt.Conn) holding the conn."},
	/* set_data */
	{"set_data", (PyCFunction) py_myqtt_conn_set_data, METH_VARARGS,
	 "Allows to sent arbitary object references index by a name associated to the conn. This API is set on top myqtt_conn_set_data_full."},
	/* get_data */
	{"get_data", (PyCFunction) py_myqtt_conn_get_data, METH_VARARGS,
	 "Allows to retrieve arbitary object references index by a name associated to the conn that were configured with Conn.set_data. This API is set on top myqtt_conn_set_data_full."},
	/* skip_conn_close */
	{"skip_conn_close", (PyCFunction) py_myqtt_conn_skip_conn_close, METH_VARARGS,
	 "Allows to configure this myqtt.Conn object to not close automatically its associated reference when the object is collected. Calling without arguments activates skip conn close on dealloction, otherwise pass skip=False to leave default behaviour."},
	/* block */
	{"block", (PyCFunction) py_myqtt_conn_block, METH_VARARGS,
	 "Allows to receiving any content from the provided conn. This method uses myqtt_conn_block."},
	/* is_blocked */
	{"is_blocked", (PyCFunction) py_myqtt_conn_is_blocked, METH_VARARGS,
	 "Allows to check blocked status applied by myqtt.Conn.block method."},
 	{NULL}  
}; 


static PyTypeObject PyMyQttConnType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size*/
    "myqtt.Conn",              /* tp_name*/
    sizeof(PyMyQttConn),       /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)py_myqtt_conn_dealloc, /* tp_dealloc*/
    0,                         /* tp_print*/
    0,                         /* tp_getattr*/
    0,                         /* tp_setattr*/
    0,                         /* tp_compare*/
    0,                         /* tp_repr*/
    0,                         /* tp_as_number*/
    0,                         /* tp_as_sequence*/
    0,                         /* tp_as_mapping*/
    0,                         /* tp_hash */
    0,                         /* tp_call*/
    0,                         /* tp_str*/
    py_myqtt_conn_get_attr,    /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags*/
    "myqtt.Conn, the object used to represent a connected MQTT session.",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    py_myqtt_conn_methods,     /* tp_methods */
    0, /* py_myqtt_conn_members, */ /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)py_myqtt_conn_init_type,      /* tp_init */
    0,                         /* tp_alloc */
    py_myqtt_conn_new,  /* tp_new */

};

/** 
 * @brief Allows to create a new PyMyQttConn instance using the
 * reference received.
 *
 * NOTE: At server side notification use
 * py_myqtt_conn_find_reference to avoid creating/finishing
 * references for each notification.
 *
 * @param conn The conn to use as reference to wrap
 *
 * @param acquire_ref Allows to configure if py_conn reference must
 * acquire a reference to the conn.
 *
 * @param close_ref Allows to signal the object created to close or
 * not the conn when the reference is garbage collected.
 *
 * @return A newly created PyMyQttConn reference.
 */
PyObject * py_myqtt_conn_create   (MyQttConn  * conn, 
				   axl_bool     acquire_ref,
				   axl_bool     close_ref)
{
	/* return a new instance */
	PyMyQttConn * obj = (PyMyQttConn *) PyObject_CallObject ((PyObject *) &PyMyQttConnType, NULL); 

	/* check ref created */
	if (obj == NULL) {
		py_myqtt_log (PY_MYQTT_CRITICAL, "Failed to create PyMyQttConn object, returning NULL");
		return NULL;
	} /* end if */

	/* configure close_ref */
	obj->close_ref = close_ref;

	/* set channel reference received */
	if (obj && conn) {
		/* check to acquire a ref */
		if (acquire_ref) {
			py_myqtt_log (PY_MYQTT_DEBUG, "acquiring a reference to conn: %p (role: %s)",
				       conn, __py_myqtt_conn_stringify_role (conn));
			/* check ref */
			if (! myqtt_conn_ref_internal (conn, "py_myqtt_conn_create", axl_false)) {
				py_myqtt_log (PY_MYQTT_CRITICAL, "failed to acquire reference, unable to create conn");
				Py_DECREF (obj);
				return NULL;
			}
		} /* end if */

		/* configure the reference */
		obj->conn = conn;
	} /* end if */

	/* return object */
	return (PyObject *) obj;
}

/** 
 * @internal Function used to reuse PyMyQttConn references
 * rather creating and finishing them especially at server side async
 * notification.
 *
 * This function is designed to avoid using
 * py_myqtt_conn_create providing a way to reuse references
 * that, not only saves memory, but are available after finishing the
 * python context that created the particular conn reference.
 *
 * @param conn The conn for which its reference will be looked up.
 *
 * @param py_ctx The myqtt.Ctx object where to lookup for an already
 * created myqtt.Conn reference.
 */
PyObject * py_myqtt_conn_find_reference (MyQttConn * conn)
{
	return py_myqtt_conn_create (
		/* conn to wrap */
		conn, 
		/* acquire a reference to the conn */
		axl_true,  
		/* do not close the conn when the reference is collected, close_ref=axl_false */
		axl_false);
}

/** 
 * @brief Allows to store a python object into the provided
 * myqtt.Conn object, incrementing the reference count. The
 * object is automatically removed when the myqtt.Conn
 * reference is collected.
 */
void        py_myqtt_conn_register (PyObject   * py_conn, 
					   PyObject   * data,
					   const char * key,
					   ...)
{
	va_list    args;
	char     * full_key;

	/* check data received */
	if (key == NULL || py_conn == NULL)
		return;

	va_start (args, key);
	full_key = axl_strdup_printfv (key, args);
	va_end   (args);

	/* check to remove */
	if (data == NULL) {
		myqtt_conn_set_data (((PyMyQttConn *)py_conn)->conn, full_key, NULL);
		axl_free (full_key);
		return;
	} /* end if */
	
	/* now register the data received into the key created */
	py_myqtt_log (PY_MYQTT_DEBUG, "registering key %s = %p on myqtt.Conn %p",
		       full_key, data, py_conn);
	Py_INCREF (data);
	myqtt_conn_set_data_full (((PyMyQttConn *)py_conn)->conn, full_key, data, axl_free, (axlDestroyFunc) py_myqtt_decref);
	return;
}


/** 
 * @brief Allows to get the object associated to the key provided. The
 * reference returned is still owned by the internal hash. Use
 * Py_INCREF in the case a new reference must owned by the caller.
 */
PyObject  * py_myqtt_conn_register_get (PyObject * py_conn,
					       const char * key,
					       ...)
{
	va_list    args;
	char     * full_key;
	PyObject * data;

	/* check data received */
	if (key == NULL || py_conn == NULL) {
		py_myqtt_log (PY_MYQTT_CRITICAL, "Failed to register data, key %p or myqtt.Conn %p reference is null",
			       key, py_conn);
		return NULL;
	} /* end if */

	va_start (args, key);
	full_key = axl_strdup_printfv (key, args);
	va_end   (args);
	
	/* now register the data received into the key created */
	data = __PY_OBJECT (myqtt_conn_get_data (((PyMyQttConn *)py_conn)->conn, full_key));
	py_myqtt_log (PY_MYQTT_DEBUG, "returning key %s = %p on myqtt.Conn %p",
		       full_key, data, py_conn);
	axl_free (full_key);
	return data;
}

/** 
 * @brief Allows to check if the PyObject received represents a
 * PyMyQttConn reference.
 */
axl_bool             py_myqtt_conn_check    (PyObject          * obj)
{
	/* check null references */
	if (obj == NULL)
		return axl_false;

	/* return check result */
	return PyObject_TypeCheck (obj, &PyMyQttConnType);
}

/** 
 * @brief Inits the myqtt conn module. It is implemented as a type.
 */
void init_myqtt_conn (PyObject * module) 
{
    
	/* register type */
	if (PyType_Ready(&PyMyQttConnType) < 0)
		return;
	
	Py_INCREF (&PyMyQttConnType);
	PyModule_AddObject(module, "Conn", (PyObject *)&PyMyQttConnType);

	return;
}

