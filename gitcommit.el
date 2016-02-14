(require 'types)

(defmacro with-directory (directory &rest body)
  `(let ((old default-directory))
     (unwind-protect
         (progn
           (cd ,directory)
           ,@body)
       (cd old))))

(defun maybe-git-commit ()
  (interactive)
  (let* ((buffer (get-buffer-create "*Git Commit Thingy*"))
		 (process (get-buffer-process buffer)))
	(message "Process %s %s" process (buffer-file-name))
	(setenv "file" (buffer-file-name))
	(start-process "Git Commit Thingy"
				   buffer
				   (expand-file-name "~/code/git/maybecommit.py"))
	(setenv "file")))

(defun gitcommit-enhooken ()
  (set (make-local-variable 'backup-inhibited) t)
  (add-hook 'after-save-hook 'maybe-git-commit nil t))

(dolist (type (append edity-types programmy-types))
  (add-hook (type->hook type) 'gitcommit-enhooken))

(provide 'gitcommit)
