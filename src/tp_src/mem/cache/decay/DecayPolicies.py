from m5.params import *
from m5.SimObject import *
from m5.objects.ClockedObject import ClockedObject


class BaseDecayPolicy(ClockedObject):
    type = "BaseDecayPolicy"
    abstract = True
    cxx_class = "gem5::tp::decay_policy::Base"
    cxx_header = "tp_src/mem/cache/decay/base.hh"


class IATAC(BaseDecayPolicy):
    type = "IATAC"
    cxx_class = "gem5::tp::decay_policy::IATAC"
    cxx_header = "tp_src/mem/cache/decay/iatac.hh"
