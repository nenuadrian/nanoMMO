#!/bin/sh
# Doxygen passes each Markdown file through this before it reads it, for the
# API docs only: GitHub shows the files as they are. The first argument is the
# repository's address on GitHub, and Doxygen adds the file's path after it.
#
# - The decisions' index becomes a page with a page under it for each of its
#   groups, and each record a page under its group (decisions.awk), through
#   the label each record's heading gets here, decision-N.
# - Each postmortem goes under the postmortems' page in the same way, through
#   a label made from its file's name.
# - The island goes under Worlds and lands, and the client in a browser under
#   Deployment. The first link to the page in its parent becomes @subpage,
#   with the link's words, to the label Doxygen gives the page,
#   md_docs_2<name>, and the page keeps its address.
# - The templates aren't pages, so links to them go to GitHub.
# - The README's cover, an image in .github inside a link, becomes the same
#   link in HTML, around the copy the docs keep beside their pages, so that it
#   shows, and without a caption that repeats what it says.
# - A link to a section of another page, such as docs/gameplay.md#items, works
#   on GitHub, but Doxygen leaves it as it is, which leads nowhere, so in the
#   API docs it goes to the page.
repo=$1
file=$2
case $file in
  */docs/decisions/README.md)
    awk -v repo="$repo" -f "$(dirname "$0")/decisions.awk" "$file" "$file"
    ;;
  */docs/decisions/[0-9][0-9][0-9][0-9]-*.md)
    sed -E '1s/^# ([0-9]+)\\?\.(.*)$/# \1.\2 {#decision-\1}/' "$file"
    ;;
  */docs/postmortems/README.md)
    sed -E -e 's/^\| \[([0-9-]+)\]\(([0-9a-z-]+)\.md\)/| @subpage postmortem-\2 "\1"/' \
      -e "s#\\(template\\.md\\)#($repo/blob/main/docs/postmortems/template.md)#" "$file"
    ;;
  */docs/postmortems/*.md)
    sed -E "1s/\$/ {#postmortem-$(basename "$file" .md)}/" "$file"
    ;;
  */docs/reliability.md)
    sed -E "s#\\(postmortems/template\\.md\\)#($repo/blob/main/docs/postmortems/template.md)#" "$file"
    ;;
  */docs/worlds.md | */docs/deployment.md)
    case $file in
      */worlds.md) child=island ;;
      *) child=web ;;
    esac
    awk -v child="$child" '
      !done && match($0, "\\[[^]]+\\]\\(" child "\\.md\\)") {
        link = substr($0, RSTART, RLENGTH)
        words = substr(link, 2, index(link, "](") - 2)
        $0 = substr($0, 1, RSTART - 1) "@subpage md_docs_2" child " \"" words "\"" substr($0, RSTART + RLENGTH)
        done = 1
      }
      { print }
    ' "$file"
    ;;
  *)
    sed -E 's#^\[!\[([^]]*)\]\(\.github/(cover\.jpg)\)\]\(([^)]*)\)$#<a href="\3"><img src="\2" alt="\1"></a>#' "$file"
    ;;
esac | sed -E 's/[.]md#[a-z0-9-]+[)]/.md)/g'
