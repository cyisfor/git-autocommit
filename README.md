Disclaimer: this is a terrible idea.

Ever spend all day making a million changes, then want to find that one thing you tried, but deleted because you didn’t think it would be needed? Gosh if only there were revision control software to handle that. Well, see the problem is people demand that you only make “meaningful” commits. You changed a file to fix an overflow error while changing another file to add an environment variable, and added a file you forgot to the Makefile? Well then, you better be ready to type an essay, because your commit message must be “fixed an overflow error in a/foo.c added an environment variable to p/d/q.c and added README.md to the Makefile.” Never mind that git already records all that information automatically, making all your typing completely useless, if you can’t think of a good meaningful commit message, then how dare you commit any changes!

So, this thing says pretty much “Forget commit messages. Preserving change history’s more important.”

Set up your editor (see gitcommit.el) to call “client” every time you finish saving a file, setting the “file” environment variable to the path. It’ll fork off “server” if necessary, and server will sit there waiting around for enough changes to accumulate in that repository. If it waits long enough, it commits anyway.

It makes an automatic commit message that’s not really useful since you can get all those stats in other ways, but just setting the message “COMMIT MESSAGE ARE DUMB HURR” seemed too visionary for our dark times. If you want a meaningful “milestone” message, you’ll probably have to say `git commit -a --allow-empty` because it thinks commits that are just a message are evil or something. Otherwise, to get a meaningful idea of what these micro-commits mean, just do `git log -p`
