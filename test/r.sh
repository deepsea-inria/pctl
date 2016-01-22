<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
split=(${1//./ })

name=${split[0]}
ext=${split[1]}

cmdline="g++ -std=gnu++11 -g -O0 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include"

if [[ $ext == "unk" ]];
then
  cmdline="${cmdline} -DHONEST -DPCTL_CILK_PLUS -fcilkplus"
fi

if [[ $ext == "norm" ]];
then
  cmdline="${cmdline} -DPCTL_CILK_PLUS -fcilkplus"
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
  cmdline="${cmdline}"
fi

<<<<<<< HEAD
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> test folder
