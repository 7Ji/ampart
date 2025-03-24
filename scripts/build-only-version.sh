[[ -e .git ]] || exit 1
tag=$(git describe --abbrev=0 --tags 2>/dev/null)
tag=${tag#v}
commit=$(git rev-list --abbrev-commit --all --max-count=1)
date=$(git log -1 --format=%cd --date=format:"%Y%m%d")
version="${tag#v}-${commit}-${date}"
[[ "$(git diff --stat)" ]] && suffix='-dirty' || suffix=''
printf '%s' "${version}${suffix}"
