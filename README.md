# Library Management System

## Project Description

The Library Management System is a menu-driven C programming project developed to manage books and library issue/return records.

The system allows the user to add, update, remove, search, and view books. It also manages book issuing and returning, automatically calculates due dates, and calculates fines for late returns.

The project uses linked lists for in-memory data management and file handling for persistent storage.

## Features

### Book Management
- Add a new book
- Automatically generate Book ID
- Update book title
- Update author name
- Update book quantity
- Remove a book
- Search books by Book ID
- Search books by Book Name
- Search books by Author Name
- View all books

### Issue and Return Management
- Issue a book to a user
- Automatically generate Issue ID
- Store User ID and User Name
- Automatically record the issue date
- Automatically calculate the due date
- Return an issued book
- Track returned and currently issued books
- Prevent the same user from issuing the same book again while it is already on loan
- Display issue and return records

### Fine Calculation
- Loan period: 7 days
- Fine rate: Rs.5 per day
- Automatically calculate the number of late days
- Automatically calculate the fine when a book is returned

### Data Storage
The system stores data using files:

- `books.dat` - Stores book information
- `issues.dat` - Stores book issue and return records

The program automatically loads saved data when it starts and can save changes during or before exiting.

## Technologies Used

- C Programming
- Structures
- Linked Lists
- Dynamic Memory Allocation
- File Handling
- String Handling
- Date and Time Functions
- Searching
- Input Validation
- Menu-Driven Programming

## Main Data Structures

### Book

Each book record contains:

- Book ID
- Book Title
- Author
- Total Quantity
- Available Quantity

### Issue

Each issue record contains:

- Issue ID
- Book ID
- User ID
- User Name
- Issue Date
- Due Date
- Return Date
- Return Status
- Fine

## Main Menu

```text
+----------------------------------------+
|         Book Management Menu           |
|----------------------------------------|
| 1. Add New Book                        |
| 2. Update Book Details                 |
| 3. Remove Book                         |
| 4. Search Book                         |
| 5. View All Books                      |
| 6. Issue Book                          |
| 7. Return Book                         |
| 8. List Issued Books                   |
| 9. Save                                |
| 10. Exit                               |
+----------------------------------------+
