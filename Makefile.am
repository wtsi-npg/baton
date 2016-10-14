
ACLOCAL_AMFLAGS= -I m4

if HAVE_SPHINX
MAYBE_DOC = doc
endif

if HAVE_CHECK
MAYBE_CHECK = tests
endif

SUBDIRS = src . $(MAYBE_CHECK) $(MAYBE_DOC)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = baton.pc

etcdir = $(sysconfdir)

if COVERAGE_ENABLED
.PHONY: coverage coverage-report clean-coverage-report check-coverage

COVERAGE_INFO_FILE = $(top_builddir)/coverage.info
COVERAGE_REPORT_DIR = $(top_builddir)/coverage

coverage-report:
	@mkdir -p $(COVERAGE_REPORT_DIR)
	@touch $(COVERAGE_INFO_FILE)
	$(LCOV) --compat-libtool --directory $(top_builddir) \
--no-external --capture --output-file $(COVERAGE_INFO_FILE)
	$(GENHTML) -o $(COVERAGE_REPORT_DIR) --config-file lcov.conf \
$(COVERAGE_INFO_FILE)

coverage: coverage-report

clean-coverage-report:
	@rm -rf $(COVERAGE_INFO_FILE)
	@rm -rf $(COVERAGE_REPORT_DIR)

clean-coverage: clean-coverage-report
	-$(LCOV) --zerocounters --directory $(top_builddir)

mostlyclean-local: clean-coverage

check-coverage: clean-coverage check coverage-report

endif # COVERAGE_ENABLED
