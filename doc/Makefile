all: pctl.html pctl.pdf

%.pdf : %.md
	pandoc $< -s -o $@

%.html : %.md
	pandoc $< -s --toc --number-sections --mathjax=http://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML -c pctl.css -o $@

clean:
	rm -f pctl.pdf pctl.html5
