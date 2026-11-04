# Rho UE calibration (tower shape map and eta-shape weights)

Calibrations for the rho-based underlying-event subtraction of the jet reconstruction
(`HIJetReco.C`, `do_rho_sub`): DetermineTowerRho (one per layer) -> TowerRhoToBackground
-> SubtractTowers. Two CDBHistos payloads per dataset and one knee payload per run:

| Payload | Read by | Content |
|---|---|---|
| MBD charge knee (CDBTTree, one per run) | both modules, `set_mbd_knee_cdb_tag` / `set_mbd_knee_path` (`MbdChargeKnee`) | Q_knee of the run (`mbdq_knee`), with the fit value, edge width, events, run and status |
| tower shape map (s-map) | `DetermineTowerRho::set_tower_shape_path` / `_cdb_tag` | s(class, layer, ieta, iphi) = <E_T,tower> / <E_T> of the layer; towers / s are used to find and rank the seed kT jets ("seed only") |
| UE calibration | `TowerRhoToBackground::set_eta_calib_path` / `_cdb_tag` | per (layer, eta strip, vertex z, Q'): multiplicative w and affine alpha, beta; UE per tower = alpha rho cosh(eta) + beta (default) or w rho cosh(eta), interpolated bilinearly in (z, Q') |

Q' = (q_S + q_N) / Q_knee(run): the MBD charge scaled by the run's knee (central edge of
the minimum-bias charge spectrum), so that one map serves every run of a dataset.
The knee varies by ~0.5% between neighbouring runs, with steps of a few % (Run 2024: the runs
up to 54495 and the t0-shifted runs are 2-3% higher) and outliers (Run 2025 run 69671: -15.6%),
all real MBD charge-scale changes that the official `CentralityScale` also shows, so it is a per-run
calibration of its own: every run that is reconstructed needs a knee payload, while the
s-map and UE payloads serve the whole dataset. The modules take the knee, in order, from
`set_mbd_knee()`, the knee payload (CDB tag, or a path to one payload or to a directory of
`mbdq_knee_<run>.root`), the per-run table `mbdq_knee` inside the s-map / UE payload
(the runs of the calibration only), or else the dataset default: the median knee of that
table, with a warning (a run off the dataset's typical scale, like 69671, then gets the wrong
Q', so every run should get its own knee). `HIJetReco.C` uses the CDB tag `HIJETS::rho_sub_knee_tag`
when the payloads come from the CDB, and the payloads' tables when they come from file paths,
unless `HIJETS::rho_sub_knee_path` is set. The knee must be fitted on the same MBD charge
that the reconstruction uses (`MbdOut`, here with TTGain 1 as in production).
Classes default to the cross-validated run 2024 Au+Au binning, 40 z x 26 Q'
(`RhoUECalibBase::default_*_edges`).

Code: `coresoftware/calibrations/jets/rho_ue_calib` (library `librhouecalib`):
`MbdChargeHistMaker`, `TowerShapeMapMaker`, `RhoUECalibMaker` (SubsysReco, common event
selection and classes in `RhoUECalibBase`), `RhoUECalibPayload` (knee fit, payload builders),
`RhoUECalibEventVeto` (event veto, below) and `MbdTTGainOverride` (MBD time-channel gain).

## Input and event selection

`DST_CALOFITTING` plus the `DST_ZDC_RAW` of the same run and segment (for the minimum-bias
classifier). The macro takes one `DST_CALOFITTING` file or a `.list` of them; a list must
hold segments of one run (the CDB timestamp is set from the run). MBD and global vertex
reconstruction and `MinimumBiasClassifier` in every step; for "smap" and "ue" also the
calorimeter calibration with `Process_Calo_Calib()` and the EMCal retowered onto the HCal grid.
Selected events: Au+Au minimum bias with an MBD vertex in the class z range (|z| < 60 cm by
default).

Event veto (`RhoUECalibEventVeto`, macro argument `veto`, on by default), in front of the
calibration modules, until the bunch-crossing and MBD charge-loss selection is done before
DST_CALOFITTING:
- vertex: an MBD vertex with |z| < 60 cm;
- main bunch crossing ("smap" and "ue"): `MinimumBiasInfo::getBunchClass() == MAIN` when the
  `MinimumBiasClassifier` ran with bunch classes (a `CentralityScale` payload with `t0offset`;
  none for Run 2024), otherwise |t0 - t0(run)| < 2.5 ns with the run's main-bunch centre from
  the knee table (column `t0`, found in the "knee" step from the t0 spectrum of every run);
- off-band pile-up (MBD charge lost in the production fits): full charge (sum of the PMT
  charges > 0) / `MbdOut` charge (PMTs with a time hit) > 1.3. This needs the MBD
  reconstructed from the waveforms by the current `MbdReco` (`mbd_refit`, on by default: the
  job also reads `DST_TRIGGERED_EVENT_seb18` of the same segments, the DST_CALOFITTING MBD
  branches are switched off, `MbdTTGainOverride` sets TTGain 1 for the production timing).
  Without the refit the two charges are equal and the cut does nothing (it is switched off).
The module also serves DST skims: `set_require_minbias`, `set_accept_satellites` (the +-5 ns
bunches when the classifier has bunch classes; in table mode widen `set_t0_window`), and
`set_tag_bunch_class` writes t0' and the bunch class (MAIN / M5 / P5 / NONE,
`set_bunch_class_windows(2.5, 7.5)`) into `MinimumBiasInfo` when the classifier left them unset.

`Fun4All_RhoUECalib_Sim.C` runs the same steps on minimum-bias HIJING DSTs (one DST or a
list; CDB tag `MDC2`, run 31, the simulation centrality scales of the Run 2024 Au+Au HIJING
production); no veto (the makers' vertex cut only).

Seed jet acceptance (`seed_abs_eta`, "ue"): < 0 (default) = the tower eta range shrunk by R, as
`DetermineTowerRho` and `HIJetReco.C` (`HIJETS::rho_sub_seed_abs_eta`); 5 = every seed jet. The
UE calibration must be made with the acceptance it is applied with.

## Batch production (condor/)

```bash
cd condor
# job list: segments with both DSTs, one job per segment
./make_todo.sh <tag, e.g. pro001_pcdb001_v001> runs.txt <listdir>
O=<output dir>
mkdir -p $O/logs/{knee,smap,ue}
condor_submit rhouecalib.job step=knee todo=<listdir>/todo.txt outdir=$O
./merge.sh knee $O     # -> $O/knee.txt, $O/knee_cdb/ (per-run knee payloads)
condor_submit rhouecalib.job step=smap todo=<listdir>/todo.txt outdir=$O knee=$O/knee.txt
./merge.sh smap $O     # -> $O/smap_payload.root
condor_submit rhouecalib.job step=ue todo=<listdir>/todo.txt outdir=$O knee=$O/knee.txt smap=$O/smap_payload.root
./merge.sh ue $O       # -> $O/rho_ue_payload.root
```

Unattended, all three steps with the merges in between (`UE_VARIANTS` adds UE steps with
other settings into `$O/<name>/`):
```bash
nohup ./run_chain.sh $O <listdir>/todo.txt setup=<script> > $O/chain.log 2>&1 &
UE_VARIANTS="ue: ue_alljets:seedeta=5" nohup ./run_chain.sh $O <listdir>/todo.txt > $O/chain.log 2>&1 &
# HIJING: lists from a file list, the sim macro
./make_todo_files.sh hijing_dsts.list <listdir> 31
nohup ./run_chain.sh $O <listdir>/todo.txt macro=Fun4All_RhoUECalib_Sim.C > $O/chain.log 2>&1 &
```

Each job writes `$O/<step>/<step>_<run>_<segment>.root` with a `.done` or `.failed` marker,
and logs to `$O/logs/<step>/`. `merge.sh` stops if any job of the step failed or has not
finished (`FORCE=1` merges the finished ones). Optional submit variables: `cdbtag=` (default
`newcdbtag`) and `setup=<script>` to source instead of the "new" build, e.g. for a local
install of `rhouecalib` / `jetbackground`. All paths must be absolute.

## Steps by hand

Every job writes sums only, so the outputs of a step are merged with `hadd`.

```bash
# 1. MBD charge knee per run
root -l -b -q 'Fun4All_RhoUECalib.C(0, "<DST_CALOFITTING file>", "knee_<run>_<seg>.root", "knee")'
hadd knee_all.root knee_*.root
root -l -b -q 'make_knee_table.C("knee_all.root", "knee.txt")'
# per-run knee payloads for the CDB: knee_cdb/mbdq_knee_<run>.root, knee_cdb/mbdq_knee_iov.txt
root -l -b -q 'make_knee_payloads.C("knee.txt", "knee_cdb")'

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

- Knee table columns: `run knee knee_err sigma chi2ndf status events t0 t0_rms`. Runs whose knee
  fit fails get knee -1; steps 2 and 3 abort on them (no knee), and on runs without a t0 when
  the main-bunch veto is on.
- Knee payloads (`RhoUECalibPayload::MakeKneePayloads`): CDBTTree single values `mbdq_knee` (the
  knee in use), `mbdq_knee_fit`, `mbdq_knee_sigma`, `events`, `run`, `status`. Runs whose fit
  failed, with fewer than `min_events` (1e5) events or an edge width more than `max_sigma_dev`
  (50%) off the median width get the median knee of the table and status 1. Each payload is
  valid for its own run (`mbdq_knee_iov.txt`: run, status, knee, file). The knee step needs
  only the MBD (~10 min per segment), so it can cover every run to be reconstructed, not only
  the calibration runs. A run without a knee payload uses the payload table, then the dataset
  default (median knee of the table, a warning at InitRun); it aborts only without any table.
- The s-map builder sets s = 1 for a whole (layer, class) when its even- and odd-event halves
  differ by more than `max_half_rms` (default 0.25), e.g. the EMCal of the most peripheral
  classes, and for towers with fewer than `min_entries` (100) good entries.
- The UE builder fits bins with at least `min_events` (50) events and fills the others from
  the nearest fitted Q' bin at the same z, then the nearest z. The payload's z and Q' bin
  contents are the event-weighted bin centres used as interpolation nodes.
- Memory: the s-map sums are 12 TH3F of 24 x 64 x n_class bins (~90 MB for 1040 classes).
- Per segment (run 2024 Au+Au, 100k events): knee ~10 min, smap ~65 min, ue ~85 min, < 0.9 GB
  memory; outputs 8 kB, 26 MB and 2.3 MB.
