TAG=$(shell git describe --always)
ifeq ($(GITHUB_ACTIONS),true)
DOCKER_REGISTRY?=ghcr.io
DOCKER_USER?=$(GITHUB_REPOSITORY_OWNER)
else
DOCKER_REGISTRY?=docker.io
DOCKER_USER?=wsinpg
endif

BATON_VERSION?=4.0.1
IRODS_VERSION?=4.2.11

NOW=$(shell date --utc --iso-8601=seconds)

DOCKER_PREFIX?=$(DOCKER_REGISTRY)/$(DOCKER_USER)
DOCKER_ARGS ?= --platform linux/amd64 --progress=plain --rm
.PHONY: clean push

image_names += ub-18.04-baton-irods-$(IRODS_VERSION)

git_url=$(shell git remote get-url origin)
git_commit=$(shell git log --pretty=format:'%H' -n 1)

images := $(addsuffix .$(TAG), $(image_names))
remote := $(addsuffix .$(TAG).pushed, $(image_names))

all: $(images)

push: $(remote)

ub-18.04-baton-irods-$(IRODS_VERSION).$(TAG): Dockerfile
	docker buildx build $(DOCKER_ARGS) \
	--load \
	--build-context source=.. \
	--build-arg DOCKER_PREFIX=$(DOCKER_PREFIX) \
	--build-arg DOCKER_IMAGE=$(subst .$(TAG),,$@) \
	--build-arg DOCKER_TAG=$(TAG) \
	--build-arg IRODS_VERSION=$(IRODS_VERSION) \
	--build-arg BATON_VERSION=$(BATON_VERSION) \
	--label org.opencontainers.image.title="iRODS $(IRODS_VERSION) baton client, Ubuntu 18.04" \
	--label org.opencontainers.image.source=$(git_url) \
	--label org.opencontainers.image.revision=$(git_commit) \
	--label org.opencontainers.image.version=$(TAG) \
	--label org.opencontainers.image.created=$(NOW) \
	--tag $(DOCKER_PREFIX)/ub-18.04-baton-irods-$(IRODS_VERSION):latest \
	--tag $(DOCKER_PREFIX)/ub-18.04-baton-irods-$(IRODS_VERSION):$(TAG) --file $< .
	touch $@

%.$(TAG).pushed: %.$(TAG)
	docker push $(DOCKER_PREFIX)/$*:$(TAG)
	docker push $(DOCKER_PREFIX)/$*:latest
	touch $@

clean:
	rm -f $(foreach image_name,$(image_names), $(image_name)*)
