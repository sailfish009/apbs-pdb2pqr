"""Basic tests to see if the code raises exceptions."""
import logging
from pathlib import Path
import pytest
import common


_LOGGER = logging.getLogger(__name__)


_LOGGER.error("Need test coverage for --userff")
_LOGGER.error("Need test coverage for --usernames")
_LOGGER.error("Need test coverage for --ligand")
_LOGGER.error("Need test coverage (with expected results) for --apbs-input")


@pytest.mark.parametrize("input_path", ["1K1I", "1AFS", "1FAS", "5DV8", "5D8V"], ids=str)
def test_basic_apo(input_path, tmp_path):
    """Basic routines on proteins without ligands."""
    args = "--log-level=INFO --ff=AMBER --drop-water"
    output_pqr = Path(input_path).stem + ".pqr"
    common.run_pdb2pqr(args=args, input_path=input_path, output_pqr=output_pqr,
                        tmp_path=tmp_path)


# @pytest.mark.parametrize("input_path", ["1K1I", "1FAS"], ids=str)
# def test_propka_apo(input_path, tmp_path):
#     """PROPKA titration of proteins without ligands."""
#     args = "--log-level=INFO --ff=AMBER --drop-water --titration-state-method=propka"
#     output_pqr = Path(input_path).stem + ".pqr"
#     run_pdb2pqr(args, input_path, output_pqr, tmp_path)


# @pytest.mark.parametrize(
#     "args, input_path, input_mol2, output_pqr",
#     [
#         pytest.param(
#             "--log-level=INFO --ff=AMBER",
#             "1HPX",
#             common.DATA_DIR / "1HPX-ligand.mol2",
#             "output.pqr",
#             id="1HPX-ligand AMBER"
#         ),
#         pytest.param(
#             "--log-level=INFO --ff=AMBER",
#             common.DATA_DIR / "1QBS.pdb",
#             common.DATA_DIR / "1QBS-ligand.mol2",
#             "output.pqr",
#             id="1QBS-ligand AMBER"
#         ),
#         pytest.param(
#             "--log-level=INFO --ff=AMBER",
#             common.DATA_DIR / "1US0.pdb",
#             common.DATA_DIR / "1US0-ligand.mol2",
#             "output.pqr",
#             id="1US0-ligand AMBER"
#         ),
#     ]
# )
# def test_ligand(args, input_path, input_mol2, output_pqr, tmp_path):
#     """Test ligand handling."""
#     args_ = "{args} --ligand={ligand}".format(args=args, ligand=input_mol2)
#     run_pdb2pqr(args_, input_path, output_pqr, tmp_path)
#     _LOGGER.warning("This test needs better checking to avoid silent failure.")