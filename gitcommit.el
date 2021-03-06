(require 'types)

(defvar-local do-git-commit t)

(defmacro with-directory (directory &rest body)
  `(let ((old default-directory))
     (unwind-protect
         (progn
           (cd ,directory)
           ,@body)
       (cd old))))

(defun maybe-git-commit ()
  (interactive)
	(when do-git-commit
		(let* ((buffer (get-buffer-create "*Git Commit Thingy*"))
					 (process (get-buffer-process buffer)))
			(setenv "add" (buffer-file-name))
			(start-process "Git Commit Thingy"
										 buffer
										 (expand-file-name "~/code/git/autocommit/client"))
			(setenv "add"))))

(defun no-git-commit ()
	(interactive)
	(setq do-git-commit nil))

(defun yes-git-commit ()
	(interactive)
	(setq do-git-commit t)
	(maybe-git-commit))

(defun gitcommit-enhooken ()
  (set (make-local-variable 'backup-inhibited) t)
  (add-hook 'after-save-hook 'maybe-git-commit nil t))

(dolist (type (append edity-types))
  (add-hook (type->hook type) 'gitcommit-enhooken))

(add-hook 'prog-mode-hook 'gitcommit-enhooken)


(provide 'gitcommit)
