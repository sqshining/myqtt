#!/usr/bin/python
# -*- coding: utf-8 -*-
#  MyQtt: A high performance open source MQTT implementation
#  Copyright (C) 2015 Advanced Software Production Line, S.L.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public License
#  as published by the Free Software Foundation; either version 2.1
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this program; if not, write to the Free
#  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
#  02111-1307 USA
#  
#  You may find a copy of the license under this software is released
#  at COPYING file. This is LGPL software: you are welcome to develop
#  proprietary applications using this library without any royalty or
#  fee but returning back any change, improvement or addition in the
#  form of source code, project image, documentation patches, etc.
#
#  For commercial support on build MQTT enabled solutions contact us:
#          
#      Postal address:
#         Advanced Software Production Line, S.L.
#         C/ Antonio Suarez Nº 10, 
#         Edificio Alius A, Despacho 102
#         Alcalá de Henares 28802 (Madrid)
#         Spain
#
#      Email address:
#         info@aspl.es - http://www.aspl.es/mqtt
#                        http://www.aspl.es/myqtt
#
import myqtt

import os
import signal
import sys

def info (msg):
    print "[ INFO  ] : " + msg

def error (msg):
    print "[ ERROR ] : " + msg

def ok (msg):
    print "[  OK   ] : " + msg

def signal_handler (signal, stackframe):
    print ("Received signal: " + str (signal))
    return

if __name__ == '__main__':

    # create a context
    ctx = myqtt.Ctx ()

    # init context
    if not ctx.init ():
        error ("Unable to init ctx, failed to start listener")
        sys.exit(-1)

    # configure signal handling
    signal.signal (signal.SIGTERM, signal_handler)
    signal.signal (signal.SIGINT, signal_handler)
    signal.signal (signal.SIGQUIT, signal_handler)

    # configure storage
    info ("Configuring MQTT storage path..")
    os.system ("find .myqtt-listener -type f -exec rm {} \\; > /dev/null 2>&1")
    ctx.storage_set_path (".myqtt-listener", 4096)

    # create a listener
    info ("Starting listener at 0.0.0.0:34010")
    listener = myqtt.create_listener (ctx, "0.0.0.0", "34010")

    # check listener started
    if not listener.is_ok ():
        error ("ERROR: failed to start listener. Maybe there is another instance running at 34010?")
        sys.exit (-1)

    # do a wait operation
    info ("waiting requests..")
    myqtt.wait_listeners (ctx, unlock_on_signal=True)

    

        
