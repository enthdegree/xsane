/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Povilas Kanapickas <povilas@radix.lt>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

#ifndef BACKEND_GENESYS_IMAGE_PIPELINE_H
#define BACKEND_GENESYS_IMAGE_PIPELINE_H

#include "genesys_image.h"

#include <algorithm>
#include <functional>
#include <memory>

class ImagePipelineNode
{
public:
    virtual ~ImagePipelineNode();

    virtual std::size_t get_width() const = 0;
    virtual std::size_t get_height() const = 0;
    virtual PixelFormat get_format() const = 0;

    std::size_t get_row_bytes() const
    {
        return get_pixel_row_bytes(get_format(), get_width());
    }

    virtual void get_next_row_data(std::uint8_t* out_data) = 0;
};

// A pipeline node that produces data from a callable
class ImagePipelineNodeCallableSource : public ImagePipelineNode
{
public:
    using ProducerCallback = std::function<void(std::size_t size, std::uint8_t* out_data)>;

    ImagePipelineNodeCallableSource(std::size_t width, std::size_t height, PixelFormat format,
                            ProducerCallback producer) :
        producer_{producer},
        width_{width},
        height_{height},
        format_{format}
    {}

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }
    PixelFormat get_format() const override { return format_; }

    void get_next_row_data(std::uint8_t* out_data) override
    {
        producer_(get_row_bytes(), out_data);
    }

private:
    ProducerCallback producer_;
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    PixelFormat format_ = PixelFormat::UNKNOWN;
};

// A pipeline node that produces data from a callable requesting fixed-size chunks.
class ImagePipelineNodeBufferedCallableSource : public ImagePipelineNode
{
public:
    using ProducerCallback = std::function<void(std::size_t size, std::uint8_t* out_data)>;

    ImagePipelineNodeBufferedCallableSource(std::size_t width, std::size_t height,
                                            PixelFormat format, std::size_t input_batch_size,
                                            ProducerCallback producer);

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }
    PixelFormat get_format() const override { return format_; }

    void get_next_row_data(std::uint8_t* out_data) override;

    std::size_t buffer_size() const { return buffer_.size(); }
    std::size_t buffer_available() const { return buffer_.available(); }

private:
    ProducerCallback producer_;
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    PixelFormat format_ = PixelFormat::UNKNOWN;

    std::size_t curr_row_ = 0;

    ImageBuffer buffer_;
};

class ImagePipelineNodeBufferedGenesysUsb : public ImagePipelineNode
{
public:
    using ProducerCallback = std::function<void(std::size_t size, std::uint8_t* out_data)>;

    ImagePipelineNodeBufferedGenesysUsb(std::size_t width, std::size_t height,
                                        PixelFormat format, std::size_t total_size,
                                        const FakeBufferModel& buffer_model,
                                        ProducerCallback producer);

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }
    PixelFormat get_format() const override { return format_; }

    void get_next_row_data(std::uint8_t* out_data) override;

    std::size_t buffer_available() const { return buffer_.available(); }

private:
    ProducerCallback producer_;
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    PixelFormat format_ = PixelFormat::UNKNOWN;

    ImageBufferGenesysUsb buffer_;
};

// A pipeline node that produces data from the given array.
class ImagePipelineNodeArraySource : public ImagePipelineNode
{
public:
    ImagePipelineNodeArraySource(std::size_t width, std::size_t height, PixelFormat format,
                                 std::vector<std::uint8_t> data);

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }
    PixelFormat get_format() const override { return format_; }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    PixelFormat format_ = PixelFormat::UNKNOWN;

    std::vector<std::uint8_t> data_;
    std::size_t next_row_ = 0;
};


// A pipeline node that converts between pixel formats
class ImagePipelineNodeFormatConvert : public ImagePipelineNode
{
public:
    ImagePipelineNodeFormatConvert(ImagePipelineNode& source, PixelFormat dst_format) :
        source_(source),
        dst_format_{dst_format}
    {}

    ~ImagePipelineNodeFormatConvert() override = default;

    std::size_t get_width() const override { return source_.get_width(); }
    std::size_t get_height() const override { return source_.get_height(); }
    PixelFormat get_format() const override { return dst_format_; }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    ImagePipelineNode& source_;
    PixelFormat dst_format_;
    std::vector<std::uint8_t> buffer_;
};

// A pipeline node that handles data that comes out of segmented sensors. Note that the width of
// the output data does not necessarily match the input data width, because in many cases almost
// all width of the image needs to be read in order to desegment it.
class ImagePipelineNodeDesegment : public ImagePipelineNode
{
public:
    ImagePipelineNodeDesegment(ImagePipelineNode& source,
                               std::size_t output_width,
                               const std::vector<unsigned>& segment_order,
                               std::size_t segment_pixels,
                               std::size_t interleaved_lines,
                               std::size_t pixels_per_chunk);

    ImagePipelineNodeDesegment(ImagePipelineNode& source,
                               std::size_t output_width,
                               std::size_t segment_count,
                               std::size_t segment_pixels,
                               std::size_t interleaved_lines,
                               std::size_t pixels_per_chunk);

    ~ImagePipelineNodeDesegment() override = default;

    std::size_t get_width() const override { return output_width_; }
    std::size_t get_height() const override { return source_.get_height() / interleaved_lines_; }
    PixelFormat get_format() const override { return source_.get_format(); }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    ImagePipelineNode& source_;
    std::size_t output_width_;
    std::vector<unsigned> segment_order_;
    std::size_t segment_pixels_ = 0;
    std::size_t interleaved_lines_ = 0;
    std::size_t pixels_per_chunk_ = 0;

    RowBuffer buffer_;
};

// A pipeline node that deinterleaves data on multiple lines
class ImagePipelineNodeDeinterleaveLines : public ImagePipelineNodeDesegment
{
public:
    ImagePipelineNodeDeinterleaveLines(ImagePipelineNode& source,
                                       std::size_t interleaved_lines,
                                       std::size_t pixels_per_chunk);
};

// A pipeline node that merges 3 mono lines into a color channel
class ImagePipelineNodeMergeMonoLines : public ImagePipelineNode
{
public:
    ImagePipelineNodeMergeMonoLines(ImagePipelineNode& source,
                                    ColorOrder color_order);

    std::size_t get_width() const override { return source_.get_width(); }
    std::size_t get_height() const override { return source_.get_height() / 3; }
    PixelFormat get_format() const override { return output_format_; }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    static PixelFormat get_output_format(PixelFormat input_format, ColorOrder order);

    ImagePipelineNode& source_;
    PixelFormat output_format_ = PixelFormat::UNKNOWN;

    RowBuffer buffer_;
};

// A pipeline node that splits a color channel into 3 mono lines
class ImagePipelineNodeSplitMonoLines : public ImagePipelineNode
{
public:
    ImagePipelineNodeSplitMonoLines(ImagePipelineNode& source);

    std::size_t get_width() const override { return source_.get_width(); }
    std::size_t get_height() const override { return source_.get_height() * 3; }
    PixelFormat get_format() const override { return output_format_; }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    static PixelFormat get_output_format(PixelFormat input_format);

    ImagePipelineNode& source_;
    PixelFormat output_format_ = PixelFormat::UNKNOWN;

    std::vector<std::uint8_t> buffer_;
    unsigned next_channel_ = 0;
};

// A pipeline node that shifts colors across lines by the given offsets
class ImagePipelineNodeComponentShiftLines : public ImagePipelineNode
{
public:
    ImagePipelineNodeComponentShiftLines(ImagePipelineNode& source,
                                         unsigned shift_r, unsigned shift_g, unsigned shift_b);

    std::size_t get_width() const override { return source_.get_width(); }
    std::size_t get_height() const override { return source_.get_height() - extra_height_; }
    PixelFormat get_format() const override { return source_.get_format(); }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    ImagePipelineNode& source_;
    std::size_t extra_height_ = 0;

    std::array<unsigned, 3> channel_shifts_;

    RowBuffer buffer_;
};

// A pipeline node that shifts pixels across lines by the given offsets (performs unstaggering)
class ImagePipelineNodePixelShiftLines : public ImagePipelineNode
{
public:
    constexpr static std::size_t MAX_SHIFTS = 2;

    ImagePipelineNodePixelShiftLines(ImagePipelineNode& source,
                                     const std::vector<std::size_t>& shifts);

    std::size_t get_width() const override { return source_.get_width(); }
    std::size_t get_height() const override { return source_.get_height() - extra_height_; }
    PixelFormat get_format() const override { return source_.get_format(); }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    ImagePipelineNode& source_;
    std::size_t extra_height_ = 0;

    std::vector<std::size_t> pixel_shifts_;

    RowBuffer buffer_;
};

// A pipeline node that extracts a sub-image from the image. Padding and cropping is done as needed.
// The class can't pad to the left of the image currently, as only positive offsets are accepted.
class ImagePipelineNodeExtract : public ImagePipelineNode
{
public:
    ImagePipelineNodeExtract(ImagePipelineNode& source,
                             std::size_t offset_x, std::size_t offset_y,
                             std::size_t width, std::size_t height);

    ~ImagePipelineNodeExtract() override;

    std::size_t get_width() const override { return width_; }
    std::size_t get_height() const override { return height_; }
    PixelFormat get_format() const override { return source_.get_format(); }

    void get_next_row_data(std::uint8_t* out_data) override;

private:
    ImagePipelineNode& source_;
    std::size_t offset_x_ = 0;
    std::size_t offset_y_ = 0;
    std::size_t width_ = 0;
    std::size_t height_ = 0;

    std::size_t current_line_ = 0;
    std::vector<uint8_t> cached_line_;
};

class ImagePipelineStack
{
public:
    ImagePipelineStack() {}

    std::size_t get_input_width() const;
    std::size_t get_input_height() const;
    PixelFormat get_input_format() const;
    std::size_t get_input_row_bytes() const;

    std::size_t get_output_width() const;
    std::size_t get_output_height() const;
    PixelFormat get_output_format() const;
    std::size_t get_output_row_bytes() const;

    void clear()
    {
        nodes_.clear();
    }

    template<class Node, class... Args>
    void push_first_node(Args&&... args)
    {
        if (!nodes_.empty()) {
            throw SaneException("Trying to append first node when there are existing nodes");
        }
        nodes_.emplace_back(std::unique_ptr<Node>(new Node(std::forward<Args>(args)...)));
    }

    template<class Node, class... Args>
    void push_node(Args&&... args)
    {
        ensure_node_exists();
        nodes_.emplace_back(std::unique_ptr<Node>(new Node(*nodes_.back(),
                                                           std::forward<Args>(args)...)));
    }

    void get_next_row_data(std::uint8_t* out_data)
    {
        nodes_.back()->get_next_row_data(out_data);
    }

    std::vector<std::uint8_t> get_all_data();

private:
    void ensure_node_exists() const;

    std::vector<std::unique_ptr<ImagePipelineNode>> nodes_;
};


#endif // ifndef BACKEND_GENESYS_IMAGE_PIPELINE_H