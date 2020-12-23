#
# Copyright (c) 2018 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA. 
#

"""
  Defaults - manage Albireo/VDO defaults

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/Defaults.py#7 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from . import Constants, MgmntUtils, SizeString, UserExitStatus
import os
import re
import stat

class ArgumentError(UserExitStatus, Exception):
  """Exception raised to indicate an error with an argument."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, msg, *args, **kwargs):
    super(ArgumentError, self).__init__(*args, **kwargs)
    self._msg = msg

  ######################################################################
  def __str__(self):
    return self._msg

########################################################################
class Defaults(object):
  """Defaults manages default values for arguments."""

  NOTSET = -1
  ackThreads = 1
  ackThreadsMax = 100
  ackThreadsMin = 0
  activate = Constants.enabled
  bioRotationInterval = 64
  bioRotationIntervalMax = 1024
  bioRotationIntervalMin = 1
  bioThreadOverheadMB = 18  # MB
  bioThreads = 4
  bioThreadsMax = 100
  bioThreadsMin = 1
  blockMapCacheSize = SizeString("128M")
  blockMapCacheSizeMaxPlusOne = SizeString("16T")
  blockMapCacheSizeMin = SizeString("128M")
  blockMapCacheSizeMinPerLogicalThread = 16 * MgmntUtils.MiB
  blockMapPeriod = 16380
  blockMapPeriodMax = 16380
  blockMapPeriodMin = 1
  cfreq = 0
  confFile = os.getenv('VDO_CONF_DIR', '/etc') + '/vdoconf.yml'
  compression = Constants.enabled
  deduplication = Constants.enabled
  cpuThreads = 2
  cpuThreadsMax = 100
  cpuThreadsMin = 1
  emulate512 = Constants.disabled
  hashZoneThreads = 1
  hashZoneThreadsMax = 100
  hashZoneThreadsMin = 0
  indexMem = 0.25
  indexMemIntMax = 1024
  indexMemIntMin = 1
  logicalSizeMax = SizeString("4096T")
  logicalThreads = 1
  logicalThreadsBlockMapCacheSizeThreshold = 9
  logicalThreadsMax = 60
  logicalThreadsMin = 0
  maxDiscardSize = SizeString("4K")
  maxDiscardSizeMaxPlusOne = SizeString("4G");
  maxDiscardSizeMin = SizeString("4K");
  mdRaid5Mode = 'on'
  physicalThreadOverheadMB = 10  # MB
  physicalThreads = 1
  physicalThreadsMax = 16
  physicalThreadsMin = 0
  slabSize = SizeString("2G")
  slabSizeMax = SizeString("32G")
  slabSizeMin = SizeString("128M")
  sparseIndex = Constants.disabled
  udsParallelFactor = 0
  vdoPhysicalBlockSize = 4096
  vdoLogLevel = 'info'
  vdoLogLevelChoices = ['critical', 'error', 'warning', 'notice', 'info',
                        'debug']
  vdoTargetName = 'vdo'
  writePolicy = 'auto'
  writePolicyChoices = ['async', 'sync', 'auto']

  ######################################################################
  # Public methods
  ######################################################################
  @staticmethod
  def checkAbspath(value):
    """Checks that an option is an absolute pathname.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The pathname as a string.
    Raises:
      ArgumentError
    """
    if os.path.isabs(value):
      return value
    raise ArgumentError(_("must be an absolute pathname"))

  ######################################################################
  @staticmethod
  def checkBlkDev(value):
    """Checks that an option is a valid name for the backing store.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The backing store name as a string.
    Raises:
      ArgumentError
    """
    return Defaults.checkAbspath(value)

  ######################################################################
  @staticmethod
  def checkBlockMapPeriod(value):
    """Checks that an option is an acceptable value for the block map period.
    The number must be at least 1 and no bigger than 16380.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an int.
    Raises:
      ArgumentError
    """
    return Defaults._rangeCheck(Defaults.blockMapPeriodMin,
                                Defaults.blockMapPeriodMax,
                                value)

  ######################################################################
  @staticmethod
  def checkConfFile(value):
    """Checks that an option specifies a possible config file path name.

    Currently the only restriction is that the path name may not refer
    to an existing block device node in the file system.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value.
    Raises:
      ArgumentError

    """
    return Defaults._checkNotBlockFile(value)

  ######################################################################
  @staticmethod
  def checkIndexmem(value):
    """Checks that an option is a legitimate index memory setting.

    To handle non-US locales while still supporting the documented
    behavior, we allow either a period or a comma as the decimal
    separator. This will be normalized to the decimal-point
    representation; internally indexMem is always either the string
    representation of an integer or one of the exact strings '0.25',
    '0.5', or '0.75' regardless of locale.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The memory setting as a string.
    Raises:
      ArgumentError

    """
    value = value.replace(",", ".")
    try:
      if value == '0.25' or value == '0.5' or value == '0.75':
        return value
      number = int(value)
      if Defaults.indexMemIntMin <= number <= Defaults.indexMemIntMax:
        return value
    except ValueError:
      pass
    raise ArgumentError(
      _("must be an integer at least {0} and less than or equal to {1}"
        " or one of the special values of 0.25, 0.5, or 0.75")
      .format(Defaults.indexMemIntMin, Defaults.indexMemIntMax))

  ######################################################################
  @staticmethod
  def checkLogFile(value):
    """Checks that an option specifies a possible log file path name.

    Currently the only restriction is that the path name may not refer
    to an existing block device node in the file system.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value.
    Raises:
      ArgumentError

    """
    return Defaults._checkNotBlockFile(value)

  ######################################################################
  @staticmethod
  def checkLogicalSize(value):
    """Checks that an option is an LVM-style size string.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      ArgumentError
    """
    ss = Defaults.checkSize(value)
    if not (ss <= Defaults.logicalSizeMax):
      raise ArgumentError(
        _("must be less than or equal to {0}").format(Defaults.logicalSizeMax))
    return ss

  ######################################################################
  @staticmethod
  def checkMaxDiscardSize(value):
    """Checks that an option is an acceptable value for max discard size.
    Max discard sizes will be rounded to a multiple of 4k, and must be at
    least 4k and less than 4GB.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      ArgumentError
    """
    ss = Defaults.checkSiSize(value)
    if (not (Defaults.maxDiscardSizeMin
             <= ss < Defaults.maxDiscardSizeMaxPlusOne)):
      raise ArgumentError(
        _("must be at least {0} and less than {1}")
          .format(Defaults.maxDiscardSizeMin,
                  Defaults.maxDiscardSizeMaxPlusOne))
    return ss
  
  ######################################################################
  @staticmethod
  def checkPageCachesz(value):
    """Checks that an option is an acceptable value for the page cache size.
    Page cache sizes will be rounded up to a multiple of the page size, and
    must be at least 128M and less than 16T.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      ArgumentError
    """
    ss = Defaults.checkSiSize(value)
    if (not (Defaults.blockMapCacheSizeMin
             <= ss < Defaults.blockMapCacheSizeMaxPlusOne)):
      raise ArgumentError(
        _("must be at least {0} and less than {1}")
          .format(Defaults.blockMapCacheSizeMin,
                  Defaults.blockMapCacheSizeMaxPlusOne))
    return ss

  ######################################################################
  @staticmethod
  def checkPhysicalThreadCount(value):
    """Checks that an option is a valid "physical" thread count.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      ArgumentError

    """
    return Defaults._rangeCheck(Defaults.physicalThreadsMin,
                                Defaults.physicalThreadsMax,
                                value)

  ######################################################################
  @staticmethod
  def checkLogicalThreadCount(value):
    """Checks that an option is a valid "logical" thread count.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      ArgumentError

    """
    return Defaults._rangeCheck(Defaults.logicalThreadsMin,
                                Defaults.logicalThreadsMax,
                                value)

  ######################################################################
  @staticmethod
  def checkRotationInterval(value):
    """Checks that an option is a valid bio rotation interval.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      ArgumentError

    """
    return Defaults._rangeCheck(Defaults.bioRotationIntervalMin,
                                Defaults.bioRotationIntervalMax,
                                value)

  ######################################################################
  @staticmethod
  def checkSiSize(value):
    """Checks that an option is an SI unit size string.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      ArgumentError
    """
    if (value[-1].isdigit()
        or (value[-1].isalpha()
            and (value[-1].lower() in Constants.lvmSiSuffixes))):
      try:
        ss = SizeString(value)
        return ss
      except ValueError:
        pass
    raise ArgumentError(_("must be an SI-style size string"))

  ######################################################################
  @staticmethod
  def checkSize(value):
    """Checks that an option is an LVM-style size string.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      ArgumentError
    """
    try:
      ss = SizeString(value)
      return ss
    except ValueError:
      pass
    raise ArgumentError(_("must be an LVM-style size string"))

  ######################################################################
  @staticmethod
  def checkSlabSize(value):
    """Checks that an option is a valid slab size.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      ArgumentError
    """
    try:
      ss = SizeString(value)
      size = ss.toBytes()
      if ((not MgmntUtils.isPowerOfTwo(size))
          or (not (Defaults.slabSizeMin <= ss <= Defaults.slabSizeMax))):
        raise ArgumentError(
          _("must be a power of two between {0} and {1}")
            .format(Defaults.slabSizeMin, Defaults.slabSizeMax))
      return ss
    except ValueError:
      pass
    raise ArgumentError(_("must be an LVM-style size string"))

  ######################################################################
  @staticmethod
  def checkThreadCount0_100(value):
    """Checks that an option is a valid thread count, for worker thread
    types requiring between 0 and 100 threads, inclusive.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      ArgumentError

    """
    return Defaults._rangeCheck(0, 100, value)

  ######################################################################
  @staticmethod
  def checkThreadCount1_100(value):
    """Checks that an option is a valid thread count, for worker thread
    types requiring between 1 and 100 threads, inclusive.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      ArgumentError

    """
    return Defaults._rangeCheck(1, 100, value)

  ######################################################################
  @staticmethod
  def checkVDOName(value):
    """Checks that an option is a valid VDO device name.

    The "dmsetup create" command will accept a lot of characters that
    could be problematic for udev or for running shell commands
    without careful quoting. For now, we permit only alphanumerics and
    a few other characters.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The device name as a string.
    Raises:
      ArgumentError

    """
    # See "whitelisted" characters in
    # https://sourceware.org/git/?p=lvm2.git;a=blob;f=libdm/libdm-common.c;h=e983b039276671cae991f9b8b81328760aacac4a;hb=HEAD
    #
    # N.B.: (1) Moved "-" last so we can put in it brackets in a
    # regexp. (2) Removed "=" so "-name=foo" ("-n ame=foo") gets an
    # error.
    allowedChars = "A-Za-z0-9#+.:@_-"
    if re.match("^[" + allowedChars + "]*$", value) is None:
      raise ArgumentError(
        _("VDO device names may only contain characters"
          " in '{0}': bad value '{1}'").format(allowedChars, value))
    if value.startswith("-"):
      raise ArgumentError(
        _("VDO device names may not start with '-': bad value '{0}'")
        .format(value))
    return value

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    pass

  ######################################################################
  @staticmethod
  def _checkNotBlockFile(value):
    """Checks that an option does not specify an existing block device.

    Arguments:
      value (str): Value provided as an argument to the option.
    Returns:
      The value.
    Raises:
      ArgumentError

    """
    if (value is not None
        and os.path.exists(value)
        and stat.S_ISBLK(os.stat(value).st_mode)):
      raise ArgumentError(_("{0} is a block device").format(value))
    return value

  ######################################################################
  @staticmethod
  def _rangeCheck(minValue, maxValue, value):
    """Checks that an option is a valid integer within a desired range.

    Arguments:
      minValue (int): The minimum acceptable value.
      maxValue (int): The maximum acceptable value.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      ArgumentError

    """
    try:
      number = int(value)
      if minValue <= number <= maxValue:
        return number
    except ValueError:
      pass
    raise ArgumentError(
      _("must be an integer at least {0} and less than or equal to {1}")
        .format(minValue, maxValue))
