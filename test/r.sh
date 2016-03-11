split=(${1//./ })

name=${split[0]}
ext=${split[1]}

cmdline="g++ -std=gnu++11 -g -O0 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include"

if [[ $ext == "unkh" ]];
then
#  cmdline="${cmdline} -DESTIMATOR_LOGGING -DHONEST -DPCTL_CILK_PLUS -fcilkplus"
  cmdline="${cmdline} -DESTIMATOR_LOGGING -DHONEST -DTIMING -DPCTL_CILK_PLUS -fcilkplus"
fi

if [[ $ext == "unko" ]];
then
#  cmdline="${cmdline} -DESTIMATOR_LOGGING -DTIMING -DOPTIMISTIC -DPCTL_CILK_PLUS -fcilkplus"
  cmdline="${cmdline} -DESTIMATOR_LOGGING -DOPTIMISTIC -DTIMING -DPCTL_CILK_PLUS -fcilkplus"
fi

if [[ $ext == "norm" ]];
then
#  cmdline="${cmdline} -DESTIMATOR_LOGGING -DTIMING -DPCTL_CILK_PLUS -fcilkplus"
  cmdline="${cmdline} -DTIMING -DPCTL_CILK_PLUS -fcilkplus"
fi

if [[ $ext == "log" ]];
then
  cmdline="${cmdline} -DPCTL_CILK_PLUS -fcilkplus -DLOGGING"
fi

if [[ $ext == "par" ]];
then
  cmdline="${cmdline} -DPCTL_CILK_PLUS -fcilkplus -DPCTL_PARALLEL_ELISION"
fi

if [[ $ext == "seqel" ]];
then
  cmdline="${cmdline} -DPCTL_SEQUENTIAL_ELISION"
fi

if [[ $ext == "seq" ]];
then
  cmdline="${cmdline} -DPCTL_SEQUENTIAL_BASELINE"
fi

eval ${cmdline} ${name}.cpp -o ${name}.${ext}
