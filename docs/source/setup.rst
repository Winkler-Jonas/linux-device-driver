Setup
#####

Requirements
************

Hardware
========

1. Raspberry Pi Zero
2. Rainbow HAT

Access via SSH
--------------

There is multiple ways to find the IP-Address of the Raspberry. Here is one example of doing so.

.. code-block:: bash
   :caption: retrieving interface name (linux default naming - must not apply to any device)

   active_interface=$(ip a | awk '/: e/{iface=$2} /state UP/ && iface {print iface; iface=""}' | sed 's/:.*//' | head -n 1)
   ip=$(sudo arp-scan --interface=$active_interface --localnet | grep -i "Raspberry" | awk '{print $1}' | head -n 1)

Once the IP-Address of the Raspberry is retrieved the device can be accessed using SSH.

.. code-block:: bash

   ssh user_name@$ip

Software
========

The following lines need to be added to ``/boot/config.txt`` on the Raspberry.

- ``dtparam=spi=on`` to enable device tree support for SPI-Drivers.
- ``dtoverlay=rainbow-hat`` to boot the device with the overlay.

Installation
************

To access the drivers either clone the repository from `GitHub`_ or `Gitlab`_ (VPN required)

.. code-block:: bash

   git clone (repository_url)

.. _GitHub: https://github.com/Winkler-Jonas/linux-device-driver
.. _Gitlab:

To compile and install all Drivers, the Device tree and the user-space application, execute the following command
within the cloned repository folder.

.. code-block:: bash

   make full_install

The device now must be restarted, because Raspberry does not allow to load device trees while running.
Once restarted, reconnect using SSH and navigate to the repository.

The user-space application will be compiled and accessible from it's root directory.

.. code-block:: bash

   ./user-space-app