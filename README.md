zlib compress messages on one topic and republish on another

This is a prototype of what might eventually be a mosquitto plugin.
the python code in this directory is an example of decompressing these messages as they arrive.

Usage:

   ./mosq-squasher -t test/in:test/out -t complex/blah/wop:out/simple -m mq.example.org

If you add a --stats=mystats then simplistic statistics are published to mystats/#,
examples below.  (--stats argument is optional, will default to "status/mosq-squasher"


Client mosqsub/13074-pojak received PUBLISH (d0, q0, r0, m0, 'status/mosq-squasher/topic_in/test/in/total', ... (1
bytes))
5
Client mosqsub/13074-pojak received PUBLISH (d0, q0, r0, m0, 'status/mosq-squasher/topic_in/test/in/successful',
... (1 bytes))
5
Client mosqsub/13074-pojak received PUBLISH (d0, q0, r0, m0, 'status/mosq-squasher/topic_in/blah/in/total', ... (1
bytes))
2
Client mosqsub/13074-pojak received PUBLISH (d0, q0, r0, m0, 'status/mosq-squasher/topic_in/blah/in/successful',
... (1 bytes))
2
Client mosqsub/13074-pojak received PUBLISH (d0, q0, r0, m0, 'status/mosq-squasher/overall/total', ... (1 bytes))
7

