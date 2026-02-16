import sys


def print_help():
    print("""
Mini Git - Commands

  init
      Initialize repository

  add <file>|.
      Stage file(s) or entire directory

  commit
      Create a commit

  status
      Show working tree status

  log
      Show commit history

  branch [name]
      List branches or create branch

  checkout <branch>
      Switch branch

  reset --soft|--mixed|--hard <commit>
      Reset current branch

  merge <branch>
      Merge branch into current

  rebase <branch>
      Rebase current branch

  stash push|apply|list
      Stash operations

  help
      Show this help message
""")


def main():

    if len(sys.argv) < 2:
        print_help()
        return

    command = sys.argv[1]
    args = sys.argv[2:]

    try:

        # ---------------- INIT ----------------
        if command == "init":
            from init import git_init
            git_init()

        # ---------------- ADD ----------------
        elif command == "add":
            from add import mini_git_add
            mini_git_add(args)

        # ---------------- COMMIT ----------------
        elif command == "commit":
            from commit import create_commit
            message = input("Commit message: ")
            create_commit(message)

        # ---------------- STATUS ----------------
        elif command == "status":
            from status import git_status
            git_status()

        # ---------------- LOG ----------------
        elif command == "log":
            from log import git_log
            git_log()

        # ---------------- BRANCH ----------------
        elif command == "branch":
            from branch import create_branch, list_branches
            if args:
                create_branch(args[0])
            else:
                list_branches()

        # ---------------- CHECKOUT ----------------
        elif command == "checkout":
            if not args:
                print("Usage: checkout <branch>")
                return
            from checkout import checkout
            checkout(args[0])

        # ---------------- RESET ----------------
        elif command == "reset":
            if len(args) != 2:
                print("Usage: reset --soft|--mixed|--hard <commit>")
                return

            mode = args[0]
            commit = args[1]

            if mode not in ("--soft", "--mixed", "--hard"):
                print("Invalid reset mode")
                return

            from reset import reset
            reset(commit, mode[2:])

        # ---------------- MERGE ----------------
        elif command == "merge":
            if not args:
                print("Usage: merge <branch>")
                return
            from merge import merge
            merge(args[0])

        # ---------------- REBASE ----------------
        elif command == "rebase":
            if not args:
                print("Usage: rebase <branch>")
                return
            from rebase import rebase
            rebase(args[0])

        # ---------------- STASH ----------------
        elif command == "stash":
            if not args:
                print("Usage: stash push|apply|list")
                return

            from stash import stash_push, stash_apply, stash_list

            if args[0] == "push":
                stash_push()
            elif args[0] == "apply":
                stash_apply()
            elif args[0] == "list":
                stash_list()
            else:
                print("Unknown stash command")

        # ---------------- HELP ----------------
        elif command == "help":
            print_help()

        else:
            print("Unknown command:", command)
            print_help()

    except Exception as e:
        print("Error:", e)


if __name__ == "__main__":
    main()
