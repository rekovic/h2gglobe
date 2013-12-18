source version.sh

./count_all_events.sh data_7TeV | tee ${version}_summary.txt
./count_all_events.sh mc_7TeV | tee -a ${version}_summary.txt
find data_7TeV mc_7TeV -name \*.done  | xargs cat | sed 's%^\\n%%' > sha1.txt

## ./count_all_events.sh data2012_RERECO | tee ${version}_summary.txt
### ./count_all_events.sh mc_Summer12_RD1 | tee -a ${version}_summary.txt
### find data2012_RERECO mc_Summer12_RD1 -name \*.done  | xargs cat | sed 's%^\\n%%' > sha1.txt
## find data2012_RERECO -name \*.done  | xargs cat | sed 's%^\\n%%' > sha1.txt

### 
### ## ./count_all_events.sh data_2012 > ${version}_summary.txt
### ###./count_all_events.sh mc_sig_summer12_s7 >> ${version}_summary.txt
### ###./count_all_events.sh mc_sig_summer12_s7_03_06 >> ${version}_summary.txt
### ###./count_all_events.sh mc_sig_summer12_s7_03_05 >> ${version}_summary.txt
### ###./count_all_events.sh mc_sig_summer12_s7_03_04 >> ${version}_summary.txt
### ###./count_all_events.sh mc_bkg_summer12_s7 >> ${version}_summary.txt
### ###./count_all_events.sh mc_bkg_summer12_s7_b >> ${version}_summary.txt

source setup.sh
cp -p ${version}.tar.gz ${version}_summary.txt sha1.txt ${storedir}
./mkLumi.py data2012_RERECO/*.dat
./wrap.sh ls -R ${storedir}
