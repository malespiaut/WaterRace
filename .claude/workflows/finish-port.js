export const meta = {
  name: 'finish-port',
  description: 'Advance the WaterRace Linux port by one step of the lowest open phase (P1-P8): plan tasks, implement, review, merge, verify',
  whenToUse: 'Run repeatedly (e.g. under /loop) until it returns status "done" or "halt".',
  phases: [
    { title: 'Status', detail: 'sync, find lowest open phase issue' },
    { title: 'Plan', detail: 'decompose phase into PR-sized task issues' },
    { title: 'Build', detail: 'implement -> review -> fix -> merge, one task at a time' },
    { title: 'Verify', detail: 'check the phase "Done when" with evidence' },
  ],
}

// args: { maxTasks?: number }  -- tasks to land per run (default 3)
const MAX_TASKS = (args && args.maxTasks) || 3
const MAX_FIX_ROUNDS = 2
const REPO = '/home/malespiaut/Code/WaterRace'

const CTX = `
You are working on the WaterRace Mac OS 9 -> Linux/OpenGL port, repo ${REPO} (GitHub remote via gh, default branch master).
The plan is ${REPO}/PORTING.md: read the sections relevant to your task. Follow its strategy (in-place port, Pomme for Mac APIs, GL 1.4 fixed-function, convert PICT->PNG once, byte-swap at read sites).
Phase issues are titled "P<n>: ..." (#2..#9). Task issues carry label "port-task" and the line "Part of #<phase>" in their body. "port-blocked" marks a task a human must look at.
Build: cmake -S <tree> -B <tree>/build && cmake --build <tree>/build -j; quick syntax check: port/check-compile.sh <outdir>. Submodule Pomme must be initialised (git submodule update --init --recursive).
Game data: "WaterRace Data/" (tracked). The original CD extraction is at ${REPO}/WaterRace-CD (untracked, read-only reference: never modify or commit it).
Display: use xvfb-run for anything that opens a window; capture frames with glReadPixels to PNG for screenshots.
Never commit to master directly. Commit messages end with "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>". PR bodies end with "🤖 Generated with [Claude Code](https://claude.com/claude-code)".
Keep changes minimal and in the style of the surrounding code.`

const STATUS = {
  type: 'object',
  properties: {
    allClosed: { type: 'boolean', description: 'true if every phase issue P1..P8 is closed' },
    phaseIssue: { type: ['integer', 'null'] },
    phaseTitle: { type: 'string' },
    doneWhen: { type: 'string', description: 'the issue "Done when" text, verbatim' },
    notes: { type: 'string' },
  },
  required: ['allClosed', 'phaseIssue', 'phaseTitle', 'doneWhen', 'notes'],
}
const PLAN = {
  type: 'object',
  properties: {
    tasks: { type: 'array', items: { type: 'object', properties: { number: { type: 'integer' }, title: { type: 'string' } }, required: ['number', 'title'] }, description: 'open, unblocked task issues for this phase, in implementation order' },
    blockedCount: { type: 'integer' },
  },
  required: ['tasks', 'blockedCount'],
}
const IMPL = {
  type: 'object',
  properties: { pr: { type: ['integer', 'null'] }, branch: { type: 'string' }, summary: { type: 'string' } },
  required: ['pr', 'branch', 'summary'],
}
const REVIEW = {
  type: 'object',
  properties: { approve: { type: 'boolean' }, blocking: { type: 'array', items: { type: 'string' } } },
  required: ['approve', 'blocking'],
}
const VERIFY = {
  type: 'object',
  properties: { phaseDone: { type: 'boolean' }, evidence: { type: 'string' }, gapIssues: { type: 'array', items: { type: 'integer' } } },
  required: ['phaseDone', 'evidence', 'gapIssues'],
}

// ---------- Status ----------
phase('Status')
const st = await agent(`${CTX}
Task: report port status. Do not change code.
1. cd ${REPO}; git fetch --all --prune. (Do not touch the user's working tree beyond fetch.)
1b. Clean up old agent worktrees: for each entry of git worktree list under ${REPO}/.claude/worktrees/, if its branch's PR is MERGED or CLOSED (gh pr list --state all --head <branch>) and git -C <wt> status --short shows no tracked changes, run git worktree remove --force <wt> (needed because of the Pomme submodule) and git branch -D <branch>. Then git worktree prune. Never remove a worktree with uncommitted tracked changes or an open PR.
2. Ensure labels exist: gh label create port-task --color 1d76db --force; gh label create port-blocked --color b60205 --force.
3. List phase issues P1..P8 (gh issue list --state all --search "in:title P"). If all are closed, allClosed=true.
4. Otherwise pick the LOWEST-numbered open phase (P1 before P2 ...). Return its number, title and its "Done when" text.
5. notes: anything odd, e.g. PRs merged into a non-master branch whose commits never reached master (check: git log origin/master..origin/<branch> for merged PR head branches).`,
  { label: 'status', schema: STATUS, effort: 'low' })

if (!st) return { status: 'halt', reason: 'status agent failed' }
if (st.allClosed) { log('All phase issues closed: port at 100%.'); return { status: 'done' } }
log(`Working on #${st.phaseIssue} ${st.phaseTitle}`)

// ---------- Plan ----------
phase('Plan')
const plan = await agent(`${CTX}
Current phase: #${st.phaseIssue} "${st.phaseTitle}". Done when: ${st.doneWhen}
Status notes: ${st.notes}
Task: make sure this phase is broken into PR-sized task issues, then return the open ones in order.
- Read the phase issue, its comments, existing task issues ("Part of #${st.phaseIssue}", label port-task, open and closed) and the current code on origin/master.
- If work for this phase is stranded on a branch that never reached master (see status notes), the FIRST task must be re-landing it on master.
- Create missing task issues with gh issue create --label port-task, body starting "Part of #${st.phaseIssue}", with a concrete "Done when" each. One task = one reviewable PR (roughly <= 600 changed lines). Do not duplicate existing tasks; close tasks that are obsolete with a comment.
- Return open task issues WITHOUT the port-blocked label, in dependency order, and the count of open blocked ones.
- If everything for the phase looks implemented, return an empty task list.`,
  { label: `plan #${st.phaseIssue}`, schema: PLAN })

if (!plan) return { status: 'halt', reason: 'plan agent failed' }

// ---------- Build ----------
phase('Build')
const todo = plan.tasks.slice(0, MAX_TASKS)
if (plan.tasks.length > todo.length) log(`${plan.tasks.length - todo.length} more tasks left for later runs`)
const merged = [], blocked = []

for (const t of todo) {   // sequential: each task builds on the previous merge
  const impl = await agent(`${CTX}
Implement task issue #${t.number} "${t.title}" (phase #${st.phaseIssue}).
- Work in your own worktree/checkout, branched from fresh origin/master, branch name port/${t.number}-<slug>. Run git submodule update --init --recursive.
- Read the issue, implement it fully, build the game, and run whatever check proves the issue's "Done when" (sanitizers -fsanitize=address,undefined where useful).
- Commit, push, open a PR to master with body "Closes #${t.number}" plus what you verified and how. Return the PR number (null if you could not produce a working change, with the reason in summary).`,
    { label: `impl #${t.number}`, phase: 'Build', isolation: 'worktree' })
  const res = impl && await agent(`Extract the result of this implementer report as JSON (pr number or null, branch, one-paragraph summary):\n\n${impl}`,
    { label: `parse #${t.number}`, phase: 'Build', schema: IMPL, effort: 'low' })

  if (!res || !res.pr) {
    blocked.push(t.number)
    // The summary is agent-written text: pass it through a file, never inline it into a shell command.
    await agent(`cd ${REPO}; gh issue edit ${t.number} --add-label port-blocked. Then write the comment text between the markers below, verbatim, to a temp file with your Write tool and post it with gh issue comment ${t.number} --body-file <that file>. Do not put the text on a command line.
<<<COMMENT
Automated port workflow could not implement this: ${(res && res.summary || 'implementer failed').replace(/[<>]/g, '')}
COMMENT>>>`,
      { label: `block #${t.number}`, phase: 'Build', effort: 'low' })
    continue
  }

  let review = null
  for (let round = 0; round <= MAX_FIX_ROUNDS; round++) {
    review = await agent(`${CTX}
Review PR #${res.pr} (task #${t.number}) as an independent, skeptical reviewer. You did not write it.
- gh pr view ${res.pr}; gh pr diff ${res.pr}; check it out in a fresh worktree, build it, run the checks the task's "Done when" requires.
- Blocking = wrong behaviour, build/sanitizer failure, data corruption risk, unmet "Done when", violation of PORTING.md strategy, unrelated changes, committed secrets or CD files. Style nits are not blocking.
- Post your review: gh pr review ${res.pr} --comment --body "...". Return approve and the blocking list.`,
      { label: `review #${res.pr} r${round}`, phase: 'Build', schema: REVIEW })
    if (!review || review.approve || round === MAX_FIX_ROUNDS) break
    await agent(`${CTX}
Fix PR #${res.pr} (branch ${res.branch}) for these blocking review points:\n- ${review.blocking.join('\n- ')}
Check out the branch in your own worktree, fix, rebuild, re-run checks, push to the same branch.`,
      { label: `fix #${res.pr} r${round}`, phase: 'Build', isolation: 'worktree' })
  }

  if (review && review.approve) {
    const m = await agent(`cd ${REPO}; gh pr merge ${res.pr} --squash --delete-branch. If it is not mergeable (conflict), rebase the branch on origin/master in a temporary worktree, rebuild, force-push with lease, then merge. Confirm issue #${t.number} is closed (close it if not). Return merged=true only if gh pr view ${res.pr} reports state MERGED.`,
      { label: `merge #${res.pr}`, phase: 'Build', effort: 'low', schema: { type: 'object', properties: { merged: { type: 'boolean' }, detail: { type: 'string' } }, required: ['merged', 'detail'] } })
    if (!m || !m.merged) return { status: 'halt', reason: `PR #${res.pr} approved but merge failed: ${m ? m.detail : 'merge agent failed'}`, merged, blocked }
    merged.push(t.number)
  } else {
    blocked.push(t.number)
    await agent(`cd ${REPO}; gh pr edit ${res.pr} --add-label port-blocked; gh issue edit ${t.number} --add-label port-blocked; gh pr comment ${res.pr} --body "Automated review did not approve after ${MAX_FIX_ROUNDS} fix rounds. Needs a human."`,
      { label: `block #${t.number}`, phase: 'Build', effort: 'low' })
  }
}

log(`Merged: ${merged.map(n => '#' + n).join(' ') || 'none'}; blocked: ${blocked.map(n => '#' + n).join(' ') || 'none'}`)

// ---------- Verify (only when nothing unblocked is left for this phase) ----------
const remaining = plan.tasks.length - merged.length - blocked.length
if (remaining > 0) return { status: 'progress', phase: st.phaseIssue, merged, blocked, remaining }
if (plan.blockedCount + blocked.length > 0) {
  return { status: 'halt', reason: `phase #${st.phaseIssue} has only port-blocked tasks left; needs a human`, merged, blocked }
}

phase('Verify')
const ver = await agent(`${CTX}
Verify phase #${st.phaseIssue} "${st.phaseTitle}" is complete. Done when: ${st.doneWhen}
- Fresh worktree on origin/master, init submodules, build (also with -fsanitize=address,undefined).
- Actually run what the "Done when" describes. For visual criteria, run under xvfb-run, capture screenshots via glReadPixels to PNG, and LOOK at them with the Read tool: reject black frames, garbage geometry, missing/corrupt textures, wrong colours. For input criteria, inject SDL events or use the game's own test hooks; for audio, confirm the mixer receives non-silent buffers.
- Be strict: default to phaseDone=false when unsure.
- If done: post the evidence (commands, outputs, screenshot description) as a comment on #${st.phaseIssue} and close it.
- If not done: create one port-task issue per gap ("Part of #${st.phaseIssue}") and return their numbers.`,
  { label: `verify #${st.phaseIssue}`, phase: 'Verify', schema: VERIFY, effort: 'high' })

if (!ver) return { status: 'halt', reason: 'verify agent failed' }
if (ver.phaseDone) return { status: 'phase-closed', phase: st.phaseIssue, merged, evidence: ver.evidence }
if (!merged.length && !ver.gapIssues.length) return { status: 'halt', reason: `phase #${st.phaseIssue} not done but no progress and no gaps filed` }
return { status: 'progress', phase: st.phaseIssue, merged, gaps: ver.gapIssues }
