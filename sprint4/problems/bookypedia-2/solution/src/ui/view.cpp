#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <cassert>
#include <iostream>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << ", " << book.publication_year;
    return out;
}

}  // namespace detail

namespace {
struct AuthorDeclined {};
}  // namespace

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book info"s,
                    std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Delete author"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edit author"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Delete book"s,
                    std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edit book"s,
                    std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

std::vector<std::string> View::ParseTags(const std::string& raw) const {
    std::vector<std::string> tags;
    boost::algorithm::split(tags, raw, boost::algorithm::is_any_of(","));
    return tags;
}

std::vector<std::string> View::GetTags(std::istream& cmd_input) const {
    output_ << "Enter tags (comma separated):"sv << std::endl;
    std::string line;
    std::getline(input_, line);
    return ParseTags(line);
}

std::optional<std::string> View::GetOrSelectAuthorId(std::istream& cmd_input) const {
    std::string author_name;
    std::getline(input_, author_name);
    boost::algorithm::trim(author_name);

    if (author_name.empty()) {
        return SelectAuthor();
    }

    auto found = use_cases_.GetAuthorByName(author_name);
    if (found) {
        return found->id;
    }

    output_ << "No author found. Do you want to add "sv << author_name << " (y/n)?"sv << std::endl;
    std::string answer;
    std::getline(input_, answer);
    boost::algorithm::trim(answer);
    if (answer != "y"s && answer != "Y"s) {
        throw AuthorDeclined{};
    }
    use_cases_.AddAuthor(author_name);
    auto new_author = use_cases_.GetAuthorByName(author_name);
    if (!new_author) {
        return std::nullopt;
    }
    return new_author->id;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        detail::AddBookParams params;
        cmd_input >> params.publication_year;
        std::getline(cmd_input, params.title);
        boost::algorithm::trim(params.title);

        output_ << "Enter author name or empty line to select from list:"sv << std::endl;

        std::optional<std::string> author_id;
        try {
            author_id = GetOrSelectAuthorId(cmd_input);
        } catch (const AuthorDeclined&) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }

        params.tags = GetTags(cmd_input);  // всегда читаем теги, чтобы не оставлять "хвост" в stdin

        if (!author_id) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        params.author_id = *author_id;

        use_cases_.AddBook(params.author_id, params.title, params.publication_year, params.tags);
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    int i = 1;
    for (const auto& book : use_cases_.GetBooks()) {
        output_ << i++ << " "sv << book.title << " by "sv << book.author_name << ", "sv
                << book.publication_year << std::endl;
    }
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            PrintVector(output_, GetAuthorBooks(*author_id));
        }
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to Show Books");
    }
    return true;
}

std::optional<detail::BookInfo> View::SelectBookByTitle(const std::string& title) const {
    auto books = use_cases_.GetBooksByTitle(title);
    if (books.empty()) {
        return std::nullopt;
    }
    if (books.size() == 1) {
        return detail::BookInfo{books[0].id, books[0].author_id, books[0].title,
                                books[0].publication_year};
    }

    int i = 1;
    for (const auto& book : books) {
        output_ << i++ << " "sv << book.title << " by "sv << book.author_name << ", "sv
                << book.publication_year << std::endl;
    }
    output_ << "Enter the book # or empty line to cancel:"sv << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }
    int idx;
    try {
        idx = std::stoi(str);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    --idx;
    if (idx < 0 || idx >= static_cast<int>(books.size())) {
        return std::nullopt;
    }
    return detail::BookInfo{books[idx].id, books[idx].author_id, books[idx].title,
                            books[idx].publication_year};
}

std::optional<detail::BookInfo> View::GetOrSelectBook(std::istream& cmd_input) const {
    std::string title;
    std::getline(cmd_input, title);
    boost::algorithm::trim(title);

    if (!title.empty()) {
        return SelectBookByTitle(title);
    }

    auto all_books = use_cases_.GetBooks();
    if (all_books.empty()) {
        return std::nullopt;
    }

    int i = 1;
    for (const auto& book : all_books) {
        output_ << i++ << " "sv << book.title << " by "sv << book.author_name << ", "sv
                << book.publication_year << std::endl;
    }
    output_ << "Enter the book # or empty line to cancel:"sv << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }
    int idx;
    try {
        idx = std::stoi(str);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    --idx;
    if (idx < 0 || idx >= static_cast<int>(all_books.size())) {
        return std::nullopt;
    }
    return detail::BookInfo{all_books[idx].id, all_books[idx].author_id, all_books[idx].title,
                            all_books[idx].publication_year};
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        auto book = GetOrSelectBook(cmd_input);
        if (!book) {
            return true;
        }
        auto details = use_cases_.GetBookDetails(book->id);
        if (!details) {
            return true;
        }
        output_ << "Title: "sv << details->title << std::endl;
        output_ << "Author: "sv << details->author_name << std::endl;
        output_ << "Publication year: "sv << details->publication_year << std::endl;
        if (!details->tags.empty()) {
            output_ << "Tags: "sv << boost::algorithm::join(details->tags, ", "s) << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    bool success = false;
    if (!name.empty()) {
        success = use_cases_.DeleteAuthorByName(name);
    } else {
        auto author_id = SelectAuthor();
        if (author_id) {
            success = use_cases_.DeleteAuthor(*author_id);
        } else {
            return true;
        }
    }
    if (!success) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    std::string name;
    std::getline(cmd_input, name);
    boost::algorithm::trim(name);

    std::optional<std::string> author_id;
    if (!name.empty()) {
        auto found = use_cases_.GetAuthorByName(name);
        if (found) {
            author_id = found->id;
        }
    } else {
        author_id = SelectAuthor();
        if (!author_id) {
            return true;
        }
    }

    if (!author_id) {
        output_ << "Failed to edit author"sv << std::endl;
        return true;
    }

    output_ << "Enter new name:"sv << std::endl;
    std::string new_name;
    std::getline(input_, new_name);
    boost::algorithm::trim(new_name);

    if (!use_cases_.EditAuthor(*author_id, new_name)) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    auto book = GetOrSelectBook(cmd_input);
    if (!book) {
        return true;
    }
    if (!use_cases_.DeleteBook(book->id)) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    auto book = GetOrSelectBook(cmd_input);
    if (!book) {
        output_ << "Book not found"sv << std::endl;
        return true;
    }

    auto details = use_cases_.GetBookDetails(book->id);
    if (!details) {
        output_ << "Book not found"sv << std::endl;
        return true;
    }

    output_ << "Enter new title or empty line to use the current one ("sv << details->title
            << "):"sv << std::endl;
    std::string new_title;
    std::getline(input_, new_title);
    boost::algorithm::trim(new_title);
    if (new_title.empty()) {
        new_title = details->title;
    }

    output_ << "Enter publication year or empty line to use the current one ("sv
            << details->publication_year << "):"sv << std::endl;
    std::string year_str;
    std::getline(input_, year_str);
    boost::algorithm::trim(year_str);
    int new_year = details->publication_year;
    if (!year_str.empty()) {
        try {
            new_year = std::stoi(year_str);
        } catch (const std::exception&) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
    }

    output_ << "Enter tags (current tags: "sv << boost::algorithm::join(details->tags, ", "s)
            << "):"sv << std::endl;
    std::string tags_line;
    std::getline(input_, tags_line);
    std::vector<std::string> new_tags = ParseTags(tags_line);

    if (!use_cases_.EditBook(book->id, new_title, new_year, new_tags)) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    return std::nullopt;
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 or author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_authors;
    for (const auto& author : use_cases_.GetAuthors()) {
        dst_authors.push_back({author.id, author.name});
    }
    return dst_authors;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> books;
    for (const auto& book : use_cases_.GetBooks()) {
        books.push_back({book.id, book.author_id, book.title, book.publication_year});
    }
    return books;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    std::vector<detail::BookInfo> books;
    for (const auto& book : use_cases_.GetAuthorBooks(author_id)) {
        books.push_back({book.id, book.author_id, book.title, book.publication_year});
    }
    return books;
}

}  // namespace ui
