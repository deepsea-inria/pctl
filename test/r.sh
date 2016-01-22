<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
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
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
<<<<<<< HEAD
=======
>>>>>>> test folder
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
>>>>>>> test folder
=======
>>>>>>> merge
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
=======
>>>>>>> showing command line
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
split=(${1//./ })

name=${split[0]}
ext=${split[1]}

cmdline="g++ -std=gnu++11 -fno-elide-constructors -g -O0 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -I ~/pctl/test/utils"

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

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> test folder
=======
>>>>>>> showing command line
=======
>>>>>>> test folder
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
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
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
<<<<<<< HEAD
<<<<<<< HEAD
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> test folder
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> showing command line
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> test folder
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
<<<<<<< HEAD
>>>>>>> test folder
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
<<<<<<< HEAD
>>>>>>> test folder
=======
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
<<<<<<< HEAD
>>>>>>> showing command line
=======
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
<<<<<<< HEAD
>>>>>>> test folder
=======
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
<<<<<<< HEAD
>>>>>>> showing command line
=======
>>>>>>> test folder
=======
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> showing command line
>>>>>>> showing command line
=======
=======
g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
>>>>>>> test folder
<<<<<<< HEAD
=======
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> test folder
<<<<<<< HEAD
>>>>>>> test folder
=======
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> showing command line
<<<<<<< HEAD
>>>>>>> showing command line
=======
=======
eval ${cmdline} ${name}.cpp -o ${name}.${ext}
>>>>>>> test folder
>>>>>>> test folder
