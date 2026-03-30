.. _doc_about_intro:

Introduction
============

Welcome to the documentation of ns-3 Continuum.
This page offers a broad overview of the project and the various parts that
compose the project.


What is the computing continuum?
--------------------------------

The computing continuum is the combination of the multiple computing paradigms used commonly
in distributed computing environments. Three computing paradigms of note included in the comuputing continuum
are cloud, fog and edge. These computing paradigms are distinctly different and each paradigm was created to meet the latency, bandwidth,
security, cost, and control requirements of specific kinds of applications. For example, AR devices commonly use edge servers
instead of cloud servers due to a need for low latency responses.

Combining the various computing paradigms allows a diverse array of distributed devices with unique characteristics
to work together to meet the demands of both current and emerging applications.

What is distrbiuted computing?
------------------------------

Distributed computing studies computer systems whose components are located on differ-
ent computers. These computer system components usually communicate over networks
to complete a certain goal. In most cases the individual computers are distributed across
a wide geographical area. In some cases such as microservices all the communicating
components may reside on a single machine.

Distributed computing plays an important role in enabling modern computing. The use
of distributed computing has allowed corporations to scale the delivery of their services
to a global level. An example is the instant messaging and VoIP social platform Discord.
At massive scale, Discord stores and serves `trillions of user messages <https://www.scylladb.com/tech-talk/how-discord-migrated-trillions-of-messages-from-cassandra-to-scylladb/>`_ , ensuring they
remain searchable and accessible. To solve this issue, Discord uses a distributed NoSQL
data store called `ScyllaDB <https://www.scylladb.com/>`_ to store there messages across thousands of servers. Storing
this many messages would not be feasible at that scale without distributed systems and
by extension the study of distributed computing systems.

What is ns-3?
-------------

`ns-3 <https://www.nsnam.org/>`_ is a discrete-event network simulator, targeted primarily for research and educational
use. ns-3 at its core is designed to meet the simulation needs of modern networking
research by providing a simulation core that is well documented, easy to use and easy
to debug. ns-3 allows users to model, design, and analyze the performance of various
network types, including wired and wireless, in a controlled and reproducible virtual
environment. ns-3 supports research on both IP and non-IP networks but due to most
users being focused on simulations related to the Internet protocol suite it's models have
become focused on simulating TCP/IP networks. ns-3 is a crucial tool for prototyping
new protocols and testing network behavior without the need for expensive physical
hardware. ns-3 is designed as a set of libraries that can be combined together and also
with other external software libraries.

What is the purpose of this project?
------------------------------------

The purpose of this project is to fill a hole in the current simulation landscape.
Currently there exists lackluster support for simulating the computing continuum in
various simulators, including ns-3.

ns-3 is extremely strong for networking simulation, supporting modern standards
such as Wi-Fi 7 and 5G. It also includes useful built-in features such as tracing,
which makes collecting experimental data straightforward. ns-3 was chosen to be
used as the base for development due to it's in depth support for modern networking
standards and modular design. By using ns-3 as the base for development it was hoped
that the result would be well documented, easy to use and easy to debug.
