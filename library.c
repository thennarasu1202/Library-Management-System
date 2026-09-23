#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define TITLE_LEN   60
#define AUTHOR_LEN  40
#define USER_LEN    40
#define LINE_LEN    512

#define BOOKS_FILE  "books.dat"
#define ISSUES_FILE "issues.dat"

#define LOAN_DAYS   7
#define FINE_RATE   5

typedef struct
{
    int d, m, y;
} Date;

typedef struct Book
{
    int id;
    char title[TITLE_LEN];
    char author[AUTHOR_LEN];
    int total;
    int available;
    struct Book *next;
} Book;

typedef struct Issue
{
    int issue_id;
    int book_id;
    int user_id;
    char user_name[USER_LEN];
    Date issue_date;
    Date due_date;
    Date return_date;
    int returned;
    long fine;
    struct Issue *next;
} Issue;

static Book *books = NULL;
static Issue *issues = NULL;
static int dirty = 0;

static void trim(char *s)
{
    char *p = s;
    size_t n;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (p != s)
        memmove(s, p, strlen(p) + 1);

    n = strlen(s);

    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

static int eof_seen = 0;

static int read_line(const char *prompt, char *buf, size_t size)
{
    size_t len;

    if (prompt)
    {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buf, (int)size, stdin) == NULL)
    {
        buf[0] = '\0';
        eof_seen = 1;
        return 0;
    }

    len = strlen(buf);

    if (len > 0 && buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }
    else
    {
        int c;

        while ((c = getchar()) != '\n' && c != EOF)
        {
        }
    }

    return 1;
}

static int read_int(const char *prompt, int lo, int hi, int *out)
{
    char buf[LINE_LEN];
    char *end;
    long v;

    for (;;)
    {
        if (!read_line(prompt, buf, sizeof buf))
            return 0;

        trim(buf);

        if (buf[0] == '\0')
        {
            puts("Value cannot be empty.");
            continue;
        }

        v = strtol(buf, &end, 10);

        if (*end != '\0')
        {
            puts("Please enter a whole number.");
            continue;
        }

        if (v < lo || v > hi)
        {
            printf("Please enter a value between %d and %d.\n", lo, hi);
            continue;
        }

        *out = (int)v;
        return 1;
    }
}

static int read_text(const char *prompt, char *dst, size_t size)
{
    char buf[LINE_LEN];
    char *bar;

    for (;;)
    {
        if (!read_line(prompt, buf, sizeof buf))
            return 0;

        trim(buf);

        if (buf[0] == '\0')
        {
            puts("This field cannot be empty.");
            continue;
        }

        while ((bar = strchr(buf, '|')) != NULL)
            *bar = ' ';

        if (strlen(buf) >= size)
            buf[size - 1] = '\0';

        strcpy(dst, buf);

        return 1;
    }
}

static char read_choice(const char *prompt)
{
    char buf[LINE_LEN];

    if (!read_line(prompt, buf, sizeof buf))
        return 'q';

    trim(buf);

    return (char)tolower((unsigned char)buf[0]);
}

static void pause_screen(void)
{
    char buf[LINE_LEN];

    read_line("\nPress Enter to continue...", buf, sizeof buf);
}

static int ci_contains(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);

    if (nl == 0)
        return 1;

    for (; *hay; hay++)
    {
        size_t i;

        for (i = 0; i < nl; i++)
        {
            if (hay[i] == '\0')
                break;

            if (tolower((unsigned char)hay[i]) !=
                tolower((unsigned char)needle[i]))
                break;
        }

        if (i == nl)
            return 1;
    }

    return 0;
}

static Date today(void)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    Date x;

    x.d = lt->tm_mday;
    x.m = lt->tm_mon + 1;
    x.y = lt->tm_year + 1900;

    return x;
}

static Date date_add(Date base, int days)
{
    struct tm tmv;
    Date r;

    memset(&tmv, 0, sizeof tmv);

    tmv.tm_mday = base.d + days;
    tmv.tm_mon = base.m - 1;
    tmv.tm_year = base.y - 1900;
    tmv.tm_hour = 12;
    tmv.tm_isdst = -1;

    mktime(&tmv);

    r.d = tmv.tm_mday;
    r.m = tmv.tm_mon + 1;
    r.y = tmv.tm_year + 1900;

    return r;
}

static long days_from_civil(int y, int m, int d)
{
    long era;
    unsigned yoe, doy, doe;

    y -= (m <= 2);

    era = (y >= 0 ? y : y - 399) / 400;

    yoe = (unsigned)(y - era * 400);

    doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);

    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return era * 146097L + (long)doe - 719468L;
}

static long date_diff(Date a, Date b)
{
    return days_from_civil(a.y, a.m, a.d) -
           days_from_civil(b.y, b.m, b.d);
}

static void date_str(Date x, char *buf)
{
    if (x.d == 0)
    {
        strcpy(buf, "-");
        return;
    }

    sprintf(buf, "%02d-%02d-%04d", x.d, x.m, x.y);
}

static Book *find_book(int id)
{
    Book *p;

    for (p = books; p; p = p->next)
    {
        if (p->id == id)
            return p;
    }

    return NULL;
}

static int next_book_id(void)
{
    int n;

    for (n = 1;; n++)
    {
        if (find_book(n) == NULL)
            return n;
    }
}

static int next_issue_id(void)
{
    int max = 0;
    Issue *p;

    for (p = issues; p; p = p->next)
    {
        if (p->issue_id > max)
            max = p->issue_id;
    }

    return max + 1;
}

static void book_append(Book *b)
{
    b->next = NULL;

    if (books == NULL)
    {
        books = b;
        return;
    }

    {
        Book *p = books;

        while (p->next)
            p = p->next;

        p->next = b;
    }
}

static void issue_append(Issue *i)
{
    i->next = NULL;

    if (issues == NULL)
    {
        issues = i;
        return;
    }

    {
        Issue *p = issues;

        while (p->next)
            p = p->next;

        p->next = i;
    }
}

static void free_all(void)
{
    while (books)
    {
        Book *n = books->next;

        free(books);
        books = n;
    }

    while (issues)
    {
        Issue *n = issues->next;

        free(issues);
        issues = n;
    }
}

static void book_header(void)
{
    puts("---------------------------------------------------------------------------");
    puts(" ID   Title                              Author               Avail/Total");
    puts("---------------------------------------------------------------------------");
}

static void book_row(const Book *b)
{
    printf("%-4d %-34.34s %-20.20s %3d/%-3d\n",
           b->id,
           b->title,
           b->author,
           b->available,
           b->total);
}

static void add_book(void)
{
    Book *b;

    puts("\n--- ADD NEW BOOK ---");

    b = (Book *)malloc(sizeof(Book));

    if (b == NULL)
    {
        puts("Out of memory.");
        return;
    }

    b->id = next_book_id();

    printf("Book ID : %d\n", b->id);

    if (!read_text("Title : ", b->title, TITLE_LEN) ||
        !read_text("Author : ", b->author, AUTHOR_LEN) ||
        !read_int("Quantity : ", 1, 999, &b->total))
    {
        free(b);
        return;
    }

    b->available = b->total;

    book_append(b);

    dirty = 1;

    printf("\nAdded : [%d] %s by %s (%d copies)\n",
           b->id,
           b->title,
           b->author,
           b->total);
}

static void edit_book_fields(Book *b)
{
    char ch;

    printf("\nEditing [%d] %s by %s (%d/%d available)\n",
           b->id,
           b->title,
           b->author,
           b->available,
           b->total);

    puts("A. Title");
    puts("B. Author");
    puts("C. Quantity");
    puts("D. Back");

    ch = read_choice("Enter Your Choice : ");

    if (ch == 'a')
    {
        char t[TITLE_LEN];

        if (!read_text("New Title : ", t, TITLE_LEN))
            return;

        strcpy(b->title, t);

        dirty = 1;

        puts("Title updated.");
    }
    else if (ch == 'b')
    {
        char a[AUTHOR_LEN];

        if (!read_text("New Author : ", a, AUTHOR_LEN))
            return;

        strcpy(b->author, a);

        dirty = 1;

        puts("Author updated.");
    }
    else if (ch == 'c')
    {
        int issued = b->total - b->available;
        int qty;

        printf("%d copy(ies) are currently issued.\n", issued);
        printf("New quantity cannot be less than %d.\n",
               issued);

        if (!read_int("New Quantity : ",
                      issued > 0 ? issued : 1,
                      999,
                      &qty))
            return;

        b->available += qty - b->total;
        b->total = qty;

        dirty = 1;

        puts("Quantity updated.");
    }
    else if (ch != 'd')
    {
        puts("Invalid choice.");
    }
}

static Book *pick_book_by_title(const char *action)
{
    char key[TITLE_LEN];
    Book *p;
    int matches = 0;
    int id;

    printf("Enter Book Name (full or part) to %s : ",
           action);

    if (!read_text("", key, TITLE_LEN))
        return NULL;

    book_header();

    for (p = books; p; p = p->next)
    {
        if (ci_contains(p->title, key))
        {
            book_row(p);
            matches++;
        }
    }

    puts("---------------------------------------------------------------------------");

    if (matches == 0)
    {
        puts("No matching book.");
        return NULL;
    }

    if (!read_int("Enter the Book ID : ",
                  1,
                  1000000,
                  &id))
        return NULL;

    p = find_book(id);

    if (p == NULL)
        puts("No book with that ID.");

    return p;
}

static void update_book(void)
{
    char ch;
    Book *b = NULL;

    if (books == NULL)
    {
        puts("\nNo books in the system.");
        return;
    }

    puts("\n--- UPDATE BOOK DETAILS ---");
    puts("A. By Book ID");
    puts("B. By Book Name");
    puts("C. Back to Main Menu");

    ch = read_choice("Enter Your Choice : ");

    if (ch == 'a')
    {
        int id;

        if (!read_int("Enter Book ID : ",
                      1,
                      1000000,
                      &id))
            return;

        b = find_book(id);

        if (b == NULL)
            printf("No book with ID %d.\n", id);
    }
    else if (ch == 'b')
    {
        b = pick_book_by_title("update");
    }
    else if (ch == 'c')
    {
        return;
    }
    else
    {
        puts("Invalid choice.");
        return;
    }

    if (b)
        edit_book_fields(b);
}

static void remove_book_node(Book *target)
{
    Book *cur = books;
    Book *prev = NULL;

    if (target->available != target->total)
    {
        puts("Cannot remove: some copies are still issued out.");
        return;
    }

    while (cur && cur != target)
    {
        prev = cur;
        cur = cur->next;
    }

    if (cur == NULL)
        return;

    if (prev == NULL)
        books = cur->next;
    else
        prev->next = cur->next;

    printf("Removed : [%d] %s\n",
           cur->id,
           cur->title);

    free(cur);

    dirty = 1;
}

static void remove_book(void)
{
    char ch;
    Book *b = NULL;

    if (books == NULL)
    {
        puts("\nNo books in the system.");
        return;
    }

    puts("\n--- REMOVE BOOK ---");
    puts("A. By Book ID");
    puts("B. By Book Name");
    puts("C. Back to Main Menu");

    ch = read_choice("Enter Your Choice : ");

    if (ch == 'a')
    {
        int id;

        if (!read_int("Enter Book ID : ",
                      1,
                      1000000,
                      &id))
            return;

        b = find_book(id);

        if (b == NULL)
            printf("No book with ID %d.\n", id);
    }
    else if (ch == 'b')
    {
        b = pick_book_by_title("remove");
    }
    else if (ch == 'c')
    {
        return;
    }
    else
    {
        puts("Invalid choice.");
        return;
    }

    if (b)
    {
        char c;

        printf("Really remove [%d] %s? (y/n) : ",
               b->id,
               b->title);

        c = read_choice("");

        if (c == 'y')
            remove_book_node(b);
        else
            puts("Cancelled.");
    }
}

static void search_book(void)
{
    char ch;

    if (books == NULL)
    {
        puts("\nNo books in the system.");
        return;
    }

    puts("\n--- SEARCH BOOK ---");
    puts("A. By Book ID");
    puts("B. By Book Name");
    puts("C. By Author Name");
    puts("D. Back to Main Menu");

    ch = read_choice("Enter Your Choice : ");

    if (ch == 'a')
    {
        int id;
        Book *b;

        if (!read_int("Enter Book ID : ",
                      1,
                      1000000,
                      &id))
            return;

        b = find_book(id);

        if (b == NULL)
        {
            printf("No book with ID %d.\n", id);
            return;
        }

        book_header();
        book_row(b);
        puts("---------------------------------------------------------------------------");
    }
    else if (ch == 'b' || ch == 'c')
    {
        char key[TITLE_LEN];
        Book *p;
        int matches = 0;

        if (!read_text(
                ch == 'b'
                    ? "Enter Book Name (full or part) : "
                    : "Enter Author Name (full or part) : ",
                key,
                TITLE_LEN))
            return;

        book_header();

        for (p = books; p; p = p->next)
        {
            int hit;

            if (ch == 'b')
                hit = ci_contains(p->title, key);
            else
                hit = ci_contains(p->author, key);

            if (hit)
            {
                book_row(p);
                matches++;
            }
        }

        puts("---------------------------------------------------------------------------");

        printf("%d match(es).\n", matches);
    }
    else if (ch != 'd')
    {
        puts("Invalid choice.");
    }
}

static void view_books(void)
{
    Book *p;
    int n = 0;

    puts("\n--- ALL BOOKS ---");

    if (books == NULL)
    {
        puts("(no books)");
        return;
    }

    book_header();

    for (p = books; p; p = p->next)
    {
        book_row(p);
        n++;
    }

    puts("---------------------------------------------------------------------------");

    printf("%d title(s)%s\n",
           n,
           dirty ? " [unsaved changes]" : "");
}

static void issue_book(void)
{
    int id;
    int uid;
    Book *b;
    Issue *r;
    char buf1[12];
    char buf2[12];

    if (books == NULL)
    {
        puts("\nNo books in the system.");
        return;
    }

    puts("\n--- ISSUE BOOK ---");

    if (!read_int("Book ID : ",
                  1,
                  1000000,
                  &id))
        return;

    b = find_book(id);

    if (b == NULL)
    {
        printf("No book with ID %d.\n", id);
        return;
    }

    if (b->available <= 0)
    {
        printf("\"%s\" has no copies available.\n",
               b->title);
        return;
    }

    if (!read_int("User ID : ",
                  1,
                  1000000,
                  &uid))
        return;

    for (r = issues; r; r = r->next)
    {
        if (!r->returned &&
            r->book_id == id &&
            r->user_id == uid)
        {
            puts("This user already has a copy of this book on loan.");
            return;
        }
    }

    r = (Issue *)malloc(sizeof(Issue));

    if (r == NULL)
    {
        puts("Out of memory.");
        return;
    }

    if (!read_text("User Name : ",
                   r->user_name,
                   USER_LEN))
    {
        free(r);
        return;
    }

    r->issue_id = next_issue_id();
    r->book_id = id;
    r->user_id = uid;
    r->issue_date = today();
    r->due_date = date_add(r->issue_date, LOAN_DAYS);

    r->return_date.d = 0;
    r->return_date.m = 0;
    r->return_date.y = 0;

    r->returned = 0;
    r->fine = 0;

    issue_append(r);

    b->available--;

    dirty = 1;

    date_str(r->issue_date, buf1);
    date_str(r->due_date, buf2);

    printf("\nIssue #%d : \"%s\" -> %s (User %d)\n",
           r->issue_id,
           b->title,
           r->user_name,
           r->user_id);

    printf("Issue Date : %s    Due Date : %s    (%d days)\n",
           buf1,
           buf2,
           LOAN_DAYS);

    printf("Copies left : %d\n",
           b->available);
}

static void return_book(void)
{
    int id;
    int uid;
    Book *b;
    Issue *r;
    long late;
    char buf[12];

    if (issues == NULL)
    {
        puts("\nNothing has been issued yet.");
        return;
    }

    puts("\n--- RETURN BOOK ---");

    if (!read_int("Book ID : ",
                  1,
                  1000000,
                  &id))
        return;

    if (!read_int("User ID : ",
                  1,
                  1000000,
                  &uid))
        return;

    for (r = issues; r; r = r->next)
    {
        if (!r->returned &&
            r->book_id == id &&
            r->user_id == uid)
            break;
    }

    if (r == NULL)
    {
        puts("No active issue record for that Book ID / User ID.");
        return;
    }

    r->return_date = today();
    r->returned = 1;

    late = date_diff(r->return_date, r->due_date);

    if (late < 0)
        late = 0;

    r->fine = late * FINE_RATE;

    b = find_book(id);

    if (b)
    {
        if (b->available < b->total)
            b->available++;
    }

    dirty = 1;

    date_str(r->return_date, buf);

    printf("\nReturned on %s by %s.\n",
           buf,
           r->user_name);

    if (late > 0)
    {
        printf("Late by %ld day(s). Fine = %ld x %d = Rs.%ld\n",
               late,
               late,
               FINE_RATE,
               r->fine);
    }
    else
    {
        puts("Returned on time. No fine.");
    }

    if (b)
        printf("Copies available now : %d\n",
               b->available);
}

static void list_issued(void)
{
    Issue *r;
    int n = 0;
    long total_fine = 0;

    puts("\n--- ISSUE / RETURN RECORDS ---");

    if (issues == NULL)
    {
        puts("(no records)");
        return;
    }

    puts("------------------------------------------------------------------------------------------------------");
    puts("#   Book Title                    User               Issued      Due         Returned    Fine  Status");
    puts("------------------------------------------------------------------------------------------------------");

    for (r = issues; r; r = r->next)
    {
        Book *b = find_book(r->book_id);
        char i_s[12];
        char d_s[12];
        char r_s[12];

        date_str(r->issue_date, i_s);
        date_str(r->due_date, d_s);
        date_str(r->return_date, r_s);

        printf("%-3d %-29.29s %-18.18s %-11s %-11s %-11s %5ld  %s\n",
               r->issue_id,
               b ? b->title : "(book removed)",
               r->user_name,
               i_s,
               d_s,
               r_s,
               r->fine,
               r->returned ? "RETURNED" : "OUT");

        total_fine += r->fine;
        n++;
    }

    puts("------------------------------------------------------------------------------------------------------");

    printf("%d record(s). Total fine collected : Rs.%ld\n",
           n,
           total_fine);
}

static int split_fields(char *line,
                        char **field,
                        int max)
{
    int n = 0;
    char *p = line;

    while (n < max)
    {
        field[n++] = p;

        p = strchr(p, '|');

        if (p == NULL)
            break;

        *p++ = '\0';
    }

    return n;
}

static Date parse_date(const char *s)
{
    Date x;

    x.d = 0;
    x.m = 0;
    x.y = 0;

    if (s && *s && *s != '-')
        sscanf(s, "%d-%d-%d",
               &x.d,
               &x.m,
               &x.y);

    return x;
}

static void save_all(void)
{
    FILE *fp;
    Book *b;
    Issue *r;
    int nb = 0;
    int ni = 0;

    fp = fopen(BOOKS_FILE, "w");

    if (fp == NULL)
    {
        printf("Cannot write %s\n",
               BOOKS_FILE);
        return;
    }

    for (b = books; b; b = b->next)
    {
        fprintf(fp,
                "%d|%d|%d|%s|%s\n",
                b->id,
                b->total,
                b->available,
                b->title,
                b->author);

        nb++;
    }

    fclose(fp);

    fp = fopen(ISSUES_FILE, "w");

    if (fp == NULL)
    {
        printf("Cannot write %s\n",
               ISSUES_FILE);
        return;
    }

    for (r = issues; r; r = r->next)
    {
        char i_s[12];
        char d_s[12];
        char r_s[12];

        date_str(r->issue_date, i_s);
        date_str(r->due_date, d_s);
        date_str(r->return_date, r_s);

        fprintf(fp,
                "%d|%d|%d|%d|%ld|%s|%s|%s|%s\n",
                r->issue_id,
                r->book_id,
                r->user_id,
                r->returned,
                r->fine,
                i_s,
                d_s,
                r_s,
                r->user_name);

        ni++;
    }

    fclose(fp);

    dirty = 0;

    printf("Saved %d book(s) to %s and %d record(s) to %s\n",
           nb,
           BOOKS_FILE,
           ni,
           ISSUES_FILE);
}

static void load_all(void)
{
    FILE *fp;
    char line[LINE_LEN];
    char *f[9];
    int nb = 0;
    int ni = 0;

    fp = fopen(BOOKS_FILE, "r");

    if (fp)
    {
        while (fgets(line, sizeof line, fp))
        {
            Book *b;

            line[strcspn(line, "\r\n")] = '\0';

            if (line[0] == '\0')
                continue;

            if (split_fields(line, f, 5) < 5)
                continue;

            b = (Book *)malloc(sizeof(Book));

            if (b == NULL)
                break;

            b->id = (int)strtol(f[0], NULL, 10);
            b->total = (int)strtol(f[1], NULL, 10);
            b->available = (int)strtol(f[2], NULL, 10);

            strncpy(b->title,
                    f[3],
                    TITLE_LEN - 1);

            b->title[TITLE_LEN - 1] = '\0';

            strncpy(b->author,
                    f[4],
                    AUTHOR_LEN - 1);

            b->author[AUTHOR_LEN - 1] = '\0';

            if (b->id <= 0 ||
                find_book(b->id))
            {
                free(b);
                continue;
            }

            book_append(b);

            nb++;
        }

        fclose(fp);
    }

    fp = fopen(ISSUES_FILE, "r");

    if (fp)
    {
        while (fgets(line, sizeof line, fp))
        {
            Issue *r;

            line[strcspn(line, "\r\n")] = '\0';

            if (line[0] == '\0')
                continue;

            if (split_fields(line, f, 9) < 9)
                continue;

            r = (Issue *)malloc(sizeof(Issue));

            if (r == NULL)
                break;

            r->issue_id =
                (int)strtol(f[0], NULL, 10);

            r->book_id =
                (int)strtol(f[1], NULL, 10);

            r->user_id =
                (int)strtol(f[2], NULL, 10);

            r->returned =
                (int)strtol(f[3], NULL, 10);

            r->fine =
                strtol(f[4], NULL, 10);

            r->issue_date = parse_date(f[5]);
            r->due_date = parse_date(f[6]);
            r->return_date = parse_date(f[7]);

            strncpy(r->user_name,
                    f[8],
                    USER_LEN - 1);

            r->user_name[USER_LEN - 1] = '\0';

            issue_append(r);

            ni++;
        }

        fclose(fp);
    }

    dirty = 0;

    printf("Loaded %d book(s) and %d issue record(s).\n",
           nb,
           ni);
}

static void show_menu(void)
{
    puts("\n+----------------------------------------+");
    puts("|         Book Management Menu           |");
    puts("|----------------------------------------|");
    puts("| 1. Add New Book                        |");
    puts("| 2. Update Book Details                 |");
    puts("| 3. Remove Book                         |");
    puts("| 4. Search Book                         |");
    puts("| 5. View All Books                      |");
    puts("| 6. Issue Book                          |");
    puts("| 7. Return Book                         |");
    puts("| 8. List Issued Books                   |");
    puts("| 9. Save                                |");
    puts("| 10. Exit                               |");
    puts("+----------------------------------------+");
}

int main(void)
{
    int done = 0;
    char stamp[12];
    Date t = today();

    date_str(t, stamp);

    puts("\n==================================================");
    puts("     LIBRARY MANAGEMENT SYSTEM - Book Module");
    puts("==================================================");

    printf("Today : %s | Loan period : %d days | Fine : Rs.%d/day\n",
           stamp,
           LOAN_DAYS,
           FINE_RATE);

    load_all();

    while (!done)
    {
        int choice;

        show_menu();

        if (!read_int("Enter Your Choice (1-10) : ",
                      1,
                      10,
                      &choice) ||
            eof_seen)
            break;

        switch (choice)
        {
            case 1:
                add_book();
                pause_screen();
                break;

            case 2:
                update_book();
                pause_screen();
                break;

            case 3:
                remove_book();
                pause_screen();
                break;

            case 4:
                search_book();
                pause_screen();
                break;

            case 5:
                view_books();
                pause_screen();
                break;

            case 6:
                issue_book();
                pause_screen();
                break;

            case 7:
                return_book();
                pause_screen();
                break;

            case 8:
                list_issued();
                pause_screen();
                break;

            case 9:
                save_all();
                pause_screen();
                break;

            case 10:
                if (dirty)
                {
                    char c;

                    c = read_choice(
                        "There are unsaved changes. Save before exit? (y/n) : ");

                    if (c == 'y')
                        save_all();
                }

                done = 1;
                break;

            default:
                puts("Invalid choice.");
        }
    }

    free_all();

    puts("\nLibrary system closed.");

    return 0;
}