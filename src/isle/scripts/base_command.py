r"""! \file
The base command line script for Isle.
Dispatches calls to other, specific scripts.
"""

from logging import getLogger

from .. import initialize, cli
from . import continuation

## Show at the end of help messages.
EPILOG = "See https://github.com/evanberkowitz/isle"


def _run_show(args):
    # import on demand in case matplotlib is not installed
    from .show import main
    main(args)

def _makeParser():
    # map command names to properties
    commands = {"show": ("Pulls all data it can from a file in a format supported by Isle, "
                         "prints, and visualizes that data. Supported file types are HDF5 and YAML. "
                         "Select a reporter via -r to choose which information to show.",
                         cli.addShowArgs,
                         "none"),

                "continue": ("Perform a continuation HMC run. Loads a checkpoint and produce more "
                             "configurations with the same parameters and evolver.",
                             cli.addContinueArgs,
                             "isle.hmc.log")}

    # construct a base parser
    parser = cli.makeDefaultParser("isle",
                                   description="Base utility program of Isle. Dispatches to sub-commands. "
                                   "Use -h for a sub-command to get more information.",
                                   epilog=EPILOG)

    # add all sub parsers
    subp = parser.add_subparsers(title="Commands", dest="cmd")
    for command, (description, argFn, defaultLog) in commands.items():
        subParser = subp.add_parser(command, epilog=EPILOG,
                                    description=description)
        argFn(cli.addDefaultArgs(subParser, defaultLog=defaultLog))

    return parser

def main():
    """!Run Isle's base script. Dispatches to other scripts based on command line arguments."""

    mains = {"show": _run_show,
             "continue": continuation.main}

    parser = _makeParser()
    args = initialize(parser)

    try:
        mains[args.cmd](args)
    except:
        getLogger("isle").exception("Failed to execute commad %s", args.cmd)
        exit(1)
