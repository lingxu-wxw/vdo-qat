from lib.psmonitor import PSMonitor
from lib.vdo_helper import VDO
from time import sleep

vdo = VDO('vdo-ram')
vdo.create('40G')
ps_monitor = PSMonitor('./')
ps_monitor.start()
sleep(1)
vdo.remove()
