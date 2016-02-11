split=(${1//./ })

name=${split[0]}
ext=${split[1]}

cmdline="g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include"

if [[ $ext == "unk" ]];
then
  cmdline="${cmdline} -DHONEST -DPCTL_CILK_PLUS -fcilkplus"
fi

if [[ $ext == "norm" ]];
then
  cmdline="${cmdline} -DPCTL_CILK_PLUS -fcilkplus"
fi

if [[ $ext == "par" ]];
then
  cmdline="${cmdline} -DPCTL_CILK_PLUS -fcilkplus -DPARALLEL_ELISION"
fi

if [[ $ext == "seq" ]];
then
  cmdline="${cmdline} -DPCTL_CILK_PLUS -fcilkplus -DSEQUENTIAL_ELISION"
fi



eval ${cmdline} ${name}.cpp -o ${name}.${ext}
