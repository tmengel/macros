# Rho UE calibration (tower shape map and eta-shape weights)

Calibrations for the rho-based underlying-event subtraction of the jet reconstruction
(`HIJetReco.C`, `do_rho_sub`): DetermineTowerRho (one per layer) -> TowerRhoToBackground
-> SubtractTowers. Two CDBHistos payloads per dataset:

| Payload | Read by | Content |
|---|---|---|
| tower shape map (s-map) | `DetermineTowerRho::set_tower_shape_path` / `_cdb_tag` | s(class, layer, ieta, iphi) = <E_T,tower> / <E_T> of the layer; towers / s are used to find and rank the seed kT jets ("seed only") |
| UE calibration | `TowerRhoToBackground::set_eta_calib_path` / `_cdb_tag` | per (layer, eta strip, vertex z, Q'): multiplicative w and affine alpha, beta; UE per tower = alpha rho cosh(eta) + beta (default) or w rho cosh(eta), interpolated bilinearly in (z, Q') |

Q' = (q_S + q_N) / Q_knee(run): the MBD charge scaled by the run's knee (central edge of
the minimum-bias charge spectrum), so that one map serves every run of a dataset.
Classes default to the cross-validated run 2024 Au+Au binning, 40 z x 26 Q'
(`RhoUECalibBase::default_*_edges`).

Code: `coresoftware/calibrations/jets/rho_ue_calib` (library `librhouecalib`):
`MbdChargeHistMaker`, `TowerShapeMapMaker`, `RhoUECalibMaker` (SubsysReco, common event
selection and classes in `RhoUECalibBase`) and `RhoUECalibPayload` (knee fit, payload builders).

## Input and event selection

`DST_CALOFITTING` plus the `DST_ZDC_RAW` of the same run and segment (for the minimum-bias
classifier), one job per segment. Calorimeter calibration with `Process_Calo_Calib()`, MBD
and global vertex reconstruction, `MinimumBiasClassifier`, EMCal retowered onto the HCal grid.
Selected events: Au+Au minimum bias with an MBD vertex in the class z range (|z| < 60 cm by
default). No bunch-crossing or MBD charge-loss selection: these are expected to be handled
before DST_CALOFITTING.

## Steps

Every job writes sums only, so the outputs of a step are merged with `hadd`.

```bash
# 1. MBD charge knee per run
root -l -b -q 'Fun4All_RhoUECalib.C(0, "<DST_CALOFITTING file>", "knee_<run>_<seg>.root", "knee")'
hadd knee_all.root knee_*.root
root -l -b -q 'make_knee_table.C("knee_all.root", "knee.txt")'

# 2. tower shape map
root -l -b -q 'Fun4All_RhoUECalib.C(0, "<file>", "smap_<run>_<seg>.root", "smap", "<cdb tag>", "knee.txt")'
hadd smap_all.root smap_*.root
root -l -b -q 'make_tower_shape_payload.C("smap_all.root", "smap_payload.root")'

# 3. UE calibration, with rho computed with the s-map of step 2
root -l -b -q 'Fun4All_RhoUECalib.C(0, "<file>", "ue_<run>_<seg>.root", "ue", "<cdb tag>", "knee.txt", "smap_payload.root")'
hadd ue_all.root ue_*.root
root -l -b -q 'make_rho_ue_payload.C("ue_all.root", "rho_ue_payload.root")'
```

The UE calibration must be made with the rho configuration it is applied with: the same
s-map setting (step 3 with or without `smap_payload.root`), MULT rho, the default jet
acceptance and the same tower calibration as the jet reconstruction.

## Notes

- Runs whose knee fit fails get knee -1 in the table; steps 2 and 3 abort on them (no knee).
- The s-map builder sets s = 1 for a whole (layer, class) when its even- and odd-event halves
  differ by more than `max_half_rms` (default 0.25), e.g. the EMCal of the most peripheral
  classes, and for towers with fewer than `min_entries` (100) good entries.
- The UE builder fits bins with at least `min_events` (50) events and fills the others from
  the nearest fitted Q' bin at the same z, then the nearest z. The payload's z and Q' bin
  contents are the event-weighted bin centres used as interpolation nodes.
- Memory: the s-map sums are 12 TH3F of 24 x 64 x n_class bins (~90 MB for 1040 classes).
