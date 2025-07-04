#!/bin/bash

# Video to H.264 Converter Script
# Converts videos to H.264 format for optimal hardware acceleration
# while preserving quality and using compatible containers

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if ffmpeg is installed
check_ffmpeg() {
    if ! command -v ffmpeg &> /dev/null; then
        print_error "FFmpeg is not installed. Please install it first."
        echo "On Ubuntu/Debian: sudo apt install ffmpeg"
        echo "On Fedora: sudo dnf install ffmpeg"
        echo "On Arch: sudo pacman -S ffmpeg"
        exit 1
    fi
}

# Function to get video info
get_video_info() {
    local file="$1"
    echo "$(ffprobe -v quiet -print_format json -show_streams -show_format "$file" 2>/dev/null)"
}

# Function to extract codec info
get_codec_info() {
    local file="$1"
    local stream_type="$2"  # "video" or "audio"
    
    ffprobe -v quiet -select_streams ${stream_type:0:1}:0 -show_entries stream=codec_name -of csv=p=0 "$file" 2>/dev/null || echo "unknown"
}

# Function to get container format
get_container_format() {
    local file="$1"
    ffprobe -v quiet -show_entries format=format_name -of csv=p=0 "$file" 2>/dev/null | cut -d',' -f1
}

# Function to check if codec needs conversion
needs_conversion() {
    local video_codec="$1"
    local container="$2"
    
    # Check if video is already H.264
    if [[ "$video_codec" == "h264" ]]; then
        # Check if container is compatible
        case "$container" in
            "mov"|"mp4"|"mkv"|"avi")
                return 1  # No conversion needed
                ;;
            *)
                return 0  # Container needs conversion
                ;;
        esac
    else
        return 0  # Video codec needs conversion
    fi
}

# Function to determine output container
determine_container() {
    local input_container="$1"
    local input_file="$2"
    
    case "$input_container" in
        "mov"|"mp4")
            echo "mp4"
            ;;
        "mkv"|"webm")
            echo "mkv"
            ;;
        "avi")
            echo "avi"
            ;;
        *)
            # Default to mp4 for unknown containers
            echo "mp4"
            ;;
    esac
}

# Function to determine audio codec
determine_audio_codec() {
    local current_audio="$1"
    local target_container="$2"
    
    case "$target_container" in
        "mp4")
            case "$current_audio" in
                "aac"|"mp3")
                    echo "copy"  # Keep original
                    ;;
                *)
                    echo "aac"   # Convert to AAC
                    ;;
            esac
            ;;
        "mkv")
            case "$current_audio" in
                "aac"|"mp3"|"ac3"|"dts"|"flac"|"vorbis"|"opus")
                    echo "copy"  # Keep original
                    ;;
                *)
                    echo "aac"   # Convert to AAC as fallback
                    ;;
            esac
            ;;
        "avi")
            case "$current_audio" in
                "mp3"|"ac3")
                    echo "copy"  # Keep original
                    ;;
                *)
                    echo "mp3"   # Convert to MP3
                    ;;
            esac
            ;;
        *)
            echo "aac"  # Default to AAC
            ;;
    esac
}

# Function to convert video
convert_video() {
    local input_file="$1"
    local output_file="$2"
    local video_codec="$3"
    local audio_codec="$4"
    local quality="$5"
    
    local ffmpeg_cmd="ffmpeg -i \"$input_file\""
    
    # Video encoding options
    if [[ "$video_codec" == "copy" ]]; then
        ffmpeg_cmd+=" -c:v copy"
    else
        ffmpeg_cmd+=" -c:v libx264"
        
        # Get original bitrate for intelligent encoding
        local orig_bitrate=$(ffprobe -v quiet -select_streams v:0 -show_entries stream=bit_rate -of csv=p=0 "$input_file" 2>/dev/null || echo "0")
        local target_bitrate=$((orig_bitrate * 110 / 100))  # 110% of original for safety
        
        case "$quality" in
            "high")
                ffmpeg_cmd+=" -preset medium -crf 20 -maxrate ${target_bitrate} -bufsize $((target_bitrate * 2))"
                ;;
            "medium")
                ffmpeg_cmd+=" -preset medium -crf 23 -maxrate ${target_bitrate} -bufsize $((target_bitrate * 2))"
                ;;
            "low")
                ffmpeg_cmd+=" -preset fast -crf 28 -maxrate $((target_bitrate * 80 / 100)) -bufsize $((target_bitrate))"
                ;;
            "size-optimized")
                # Use original bitrate directly
                ffmpeg_cmd+=" -preset medium -b:v ${orig_bitrate} -maxrate ${orig_bitrate} -bufsize $((orig_bitrate * 2))"
                ;;
            "lossless")
                ffmpeg_cmd+=" -preset medium -crf 0"
                ;;
        esac
        
        # Add hardware acceleration if available
        if lspci | grep -i nvidia > /dev/null 2>&1; then
            print_info "NVIDIA GPU detected, trying hardware acceleration..."
            ffmpeg_cmd+=" -hwaccel cuda -hwaccel_output_format cuda"
        elif lspci | grep -i amd > /dev/null 2>&1; then
            print_info "AMD GPU detected, trying hardware acceleration..."
            ffmpeg_cmd+=" -hwaccel vaapi -hwaccel_device /dev/dri/renderD128"
        fi
    fi
    
    # Audio encoding options
    if [[ "$audio_codec" == "copy" ]]; then
        ffmpeg_cmd+=" -c:a copy"
    else
        case "$audio_codec" in
            "aac")
                ffmpeg_cmd+=" -c:a aac -b:a 192k"
                ;;
            "mp3")
                ffmpeg_cmd+=" -c:a libmp3lame -b:a 192k"
                ;;
            *)
                ffmpeg_cmd+=" -c:a $audio_codec"
                ;;
        esac
    fi
    
    # Add output file and options
    ffmpeg_cmd+=" -movflags +faststart"  # Optimize for streaming
    ffmpeg_cmd+=" -y \"$output_file\""   # Overwrite output file
    
    print_info "Running: $ffmpeg_cmd"
    echo
    
    # Execute the command
    eval $ffmpeg_cmd
}

# Function to list video files
list_video_files() {
    local files=()
    
    # Common video extensions
    local extensions=("mp4" "mov" "avi" "mkv" "webm" "flv" "wmv" "m4v" "3gp" "ogv" "ts" "mts" "m2ts")
    
    for ext in "${extensions[@]}"; do
        while IFS= read -r -d '' file; do
            files+=("$file")
        done < <(find . -maxdepth 1 -iname "*.${ext}" -type f -print0 2>/dev/null)
    done
    
    # Sort files
    IFS=$'\n' sorted_files=($(sort <<<"${files[*]}"))
    
    echo "${sorted_files[@]}"
}

# Main function
main() {
    clear
    echo -e "${BLUE}=====================================${NC}"
    echo -e "${BLUE}   Video to H.264 Converter Script${NC}"
    echo -e "${BLUE}=====================================${NC}"
    echo
    
    # Check if ffmpeg is installed
    check_ffmpeg
    
    # Get current directory
    current_dir=$(pwd)
    print_info "Working directory: $current_dir"
    echo
    
    # List video files
    print_info "Scanning for video files..."
    video_files=($(list_video_files))
    
    if [[ ${#video_files[@]} -eq 0 ]]; then
        print_error "No video files found in current directory."
        exit 1
    fi
    
    print_success "Found ${#video_files[@]} video file(s):"
    echo
    
    # Display files with info
    for i in "${!video_files[@]}"; do
        local file="${video_files[$i]}"
        local video_codec=$(get_codec_info "$file" "video")
        local audio_codec=$(get_codec_info "$file" "audio")
        local container=$(get_container_format "$file")
        
        printf "%2d) %s\n" $((i+1)) "$(basename "$file")"
        printf "     Video: %-12s Audio: %-12s Container: %s\n" "$video_codec" "$audio_codec" "$container"
        
        if needs_conversion "$video_codec" "$container"; then
            printf "     Status: ${YELLOW}Needs conversion${NC}\n"
        else
            printf "     Status: ${GREEN}Already H.264 compatible${NC}\n"
        fi
        echo
    done
    
    # File selection
    echo -e "${YELLOW}Select files to convert:${NC}"
    echo "  a) Convert all files that need conversion"
    echo "  q) Quit"
    echo "  Or enter file number(s) separated by spaces (e.g., 1 3 5)"
    echo
    read -p "Your choice: " selection
    
    case "$selection" in
        "q"|"Q")
            print_info "Exiting..."
            exit 0
            ;;
        "a"|"A")
            selected_files=()
            for i in "${!video_files[@]}"; do
                local file="${video_files[$i]}"
                local video_codec=$(get_codec_info "$file" "video")
                local container=$(get_container_format "$file")
                
                if needs_conversion "$video_codec" "$container"; then
                    selected_files+=("$file")
                fi
            done
            ;;
        *)
            selected_files=()
            for num in $selection; do
                if [[ "$num" =~ ^[0-9]+$ ]] && [[ "$num" -ge 1 ]] && [[ "$num" -le ${#video_files[@]} ]]; then
                    selected_files+=("${video_files[$((num-1))]}")
                else
                    print_warning "Invalid selection: $num"
                fi
            done
            ;;
    esac
    
    if [[ ${#selected_files[@]} -eq 0 ]]; then
        print_info "No files selected or no files need conversion."
        exit 0
    fi
    
    # Quality selection
    echo
    echo -e "${YELLOW}Select quality preset:${NC}"
    echo "  1) High quality (CRF 20) - Excellent quality, similar size to original"
    echo "  2) Medium quality (CRF 23) - Good balance (recommended)"
    echo "  3) Low quality (CRF 28) - Smaller files, lower quality"
    echo "  4) Size optimized - Match original file size closely"
    echo "  5) Lossless (CRF 0) - No quality loss, very large files"
    echo
    read -p "Your choice (1-5): " quality_choice
    
    case "$quality_choice" in
        1) quality="high" ;;
        2) quality="medium" ;;
        3) quality="low" ;;
        4) quality="size-optimized" ;;
        5) quality="lossless" ;;
        *) quality="size-optimized"; print_info "Using default: size optimized" ;;
    esac
    
    # Output directory
    echo
    read -p "Create output directory? (y/n, default: y): " create_dir
    if [[ "$create_dir" != "n" ]] && [[ "$create_dir" != "N" ]]; then
        output_dir="converted_h264"
        mkdir -p "$output_dir"
        print_info "Output directory: $output_dir"
    else
        output_dir="."
        print_info "Files will be saved in current directory with '_h264' suffix"
    fi
    
    # Process files
    echo
    print_info "Starting conversion process..."
    echo
    
    for file in "${selected_files[@]}"; do
        print_info "Processing: $(basename "$file")"
        
        # Get file info
        local video_codec=$(get_codec_info "$file" "video")
        local audio_codec=$(get_codec_info "$file" "audio")
        local container=$(get_container_format "$file")
        
        # Determine output settings
        local output_container=$(determine_container "$container" "$file")
        local output_audio=$(determine_audio_codec "$audio_codec" "$output_container")
        
        # Generate output filename
        local filename=$(basename "$file")
        local name="${filename%.*}"
        local output_file="$output_dir/${name}_h264.${output_container}"
        
        # Skip if output file exists
        if [[ -f "$output_file" ]]; then
            print_warning "Output file already exists: $output_file"
            read -p "Overwrite? (y/n): " overwrite
            if [[ "$overwrite" != "y" ]] && [[ "$overwrite" != "Y" ]]; then
                print_info "Skipping $file"
                continue
            fi
        fi
        
        # Show conversion plan
        echo "  Input:  $video_codec → $audio_codec → $container"
        echo "  Output: h264 → $output_audio → $output_container"
        echo "  File:   $output_file"
        echo
        
        # Convert
        local start_time=$(date +%s)
        
        if convert_video "$file" "$output_file" "h264" "$output_audio" "$quality"; then
            local end_time=$(date +%s)
            local duration=$((end_time - start_time))
            
            # Get file sizes
            local input_size=$(du -h "$file" | cut -f1)
            local output_size=$(du -h "$output_file" | cut -f1)
            
            print_success "Conversion completed in ${duration}s"
            print_success "Size: $input_size → $output_size"
            
            # Verify output file
            if [[ -f "$output_file" ]]; then
                local verify_codec=$(get_codec_info "$output_file" "video")
                if [[ "$verify_codec" == "h264" ]]; then
                    print_success "✓ Verified: H.264 encoding successful"
                else
                    print_error "✗ Verification failed: Output codec is $verify_codec"
                fi
            fi
        else
            print_error "Conversion failed for $file"
        fi
        
        echo
        echo "----------------------------------------"
        echo
    done
    
    print_success "All conversions completed!"
    echo
    print_info "You can now use the converted files for optimal hardware acceleration:"
    echo "  ./build/wallpaper_ne_linux \"$output_dir/your_video_h264.mp4\""
}

# Run main function
main "$@"
