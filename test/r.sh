g++ -std=gnu++11 -I ~/pctl/include -I ~/chunkedseq/include -I ~/pbbs-pctl/include -I ~/pbbs-pctl/example/include -I ~/cmdline/include -DHONEST -DPCTL_CILK_PLUS -fcilkplus $1.cpp -o $1
