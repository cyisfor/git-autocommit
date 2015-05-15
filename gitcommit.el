(require 'types)

(defmacro with-directory (directory &rest body)
  `(let ((old default-directory))
     (unwind-protect
         (progn
           (cd ,directory)
           ,@body)
       (cd old))))

(defun maybe-git-commit ()
    (shell-command
     (concat
      ". ~/code/maybegitcommit.sh "
      (buffer-file-name))))

(defun gitcommit-enhooken ()
  (set (make-local-variable 'backup-inhibited) t)
  (add-hook 'after-save-hook 'maybe-git-commit nil t))

(dolist (type (append edity-types programmy-types))
  (add-hook (type->hook type) 'gitcommit-enhooken))


(provide 'gitcommit)
