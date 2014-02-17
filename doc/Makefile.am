EXTRA_DIST = source/conf.py source/index.rst

SPHINXBUILD = sphinx-build
SPHINXOPTS = -d _build/doctrees

html-local:
	mkdir -p _static
	mkdir -p _build/doctrees
	$(SPHINXBUILD) -b html $(SPHINXOPTS) $(srcdir) _build/html

install-html-local: html
	mkdir -p $(docdir)
	cp -r _build/html $(docdir)

uninstall-local:
	rm -rf $(docdir)

clean-local:
	rm -rf _build
